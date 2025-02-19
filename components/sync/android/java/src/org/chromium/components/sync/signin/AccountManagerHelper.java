// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync.signin;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Process;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.net.NetworkChangeNotifier;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Pattern;

/**
 * AccountManagerHelper wraps our access of AccountManager in Android.
 *
 * Use the AccountManagerHelper.get(someContext) to instantiate it
 */
public class AccountManagerHelper {
    private static final String TAG = "Sync_Signin";

    private static final Pattern AT_SYMBOL = Pattern.compile("@");

    private static final String GMAIL_COM = "gmail.com";

    private static final String GOOGLEMAIL_COM = "googlemail.com";

    public static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a child account.
     */
    @VisibleForTesting
    public static final String FEATURE_IS_CHILD_ACCOUNT_KEY = "service_uca";

    private static final Object sLock = new Object();

    private static AccountManagerHelper sAccountManagerHelper;

    private final AccountManagerDelegate mAccountManager;

    private Context mApplicationContext;

    /**
     * A simple callback for getAuthToken.
     */
    public interface GetAuthTokenCallback {
        /**
         * Invoked on the UI thread if a token is provided by the AccountManager.
         *
         * @param token Auth token, guaranteed not to be null.
         */
        void tokenAvailable(String token);

        /**
         * Invoked on the UI thread if no token is available.
         *
         * @param isTransientError Indicates if the error is transient (network timeout or
         * unavailable, etc) or persistent (bad credentials, permission denied, etc).
         */
        void tokenUnavailable(boolean isTransientError);
    }

    /**
     * @param context the Android context
     * @param accountManager the account manager to use as a backend service
     */
    private AccountManagerHelper(Context context, AccountManagerDelegate accountManager) {
        mApplicationContext = context.getApplicationContext();
        mAccountManager = accountManager;
    }

    /**
     * Initialize AccountManagerHelper with a custom AccountManagerDelegate.
     * Ensures that the singleton AccountManagerHelper hasn't been created yet.
     * This can be overriden in tests using the overrideAccountManagerHelperForTests method.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @param delegate the custom AccountManagerDelegate to use.
     */
    public static void initializeAccountManagerHelper(
            Context context, AccountManagerDelegate delegate) {
        synchronized (sLock) {
            assert sAccountManagerHelper == null;
            sAccountManagerHelper = new AccountManagerHelper(context, delegate);
        }
    }

    /**
     * A getter method for AccountManagerHelper singleton which also initializes it if not wasn't
     * already initialized.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the AccountManagerHelper
     */
    public static AccountManagerHelper get(Context context) {
        synchronized (sLock) {
            if (sAccountManagerHelper == null) {
                sAccountManagerHelper = new AccountManagerHelper(
                        context, new SystemAccountManagerDelegate(context));
            }
        }
        return sAccountManagerHelper;
    }

    /**
     * Override AccountManagerHelper with a custom AccountManagerDelegate in tests.
     * Unlike initializeAccountManagerHelper, this will override the existing instance of
     * AccountManagerHelper if any. Only for use in Tests.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @param delegate the custom AccountManagerDelegate to use.
     */
    @VisibleForTesting
    public static void overrideAccountManagerHelperForTests(
            Context context, AccountManagerDelegate delegate) {
        synchronized (sLock) {
            sAccountManagerHelper = new AccountManagerHelper(context, delegate);
        }
    }

    /**
     * Creates an Account object for the given name.
     */
    public static Account createAccountFromName(String name) {
        return new Account(name, GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public List<String> getGoogleAccountNames() {
        List<String> accountNames = new ArrayList<String>();
        for (Account account : getGoogleAccounts()) {
            accountNames.add(account.name);
        }
        return accountNames;
    }

    /**
     * Retrieves a list of the Google account names on the device asynchronously.
     */
    public void getGoogleAccountNames(final Callback<List<String>> callback) {
        getGoogleAccounts(new Callback<Account[]>() {
            @Override
            public void onResult(Account[] accounts) {
                List<String> accountNames = new ArrayList<String>();
                for (Account account : accounts) {
                    accountNames.add(account.name);
                }
                callback.onResult(accountNames);
            }
        });
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public Account[] getGoogleAccounts() {
        return mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * Retrieves all Google accounts on the device asynchronously.
     */
    public void getGoogleAccounts(Callback<Account[]> callback) {
        mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE, callback);
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public boolean hasGoogleAccounts() {
        return getGoogleAccounts().length > 0;
    }

    /**
     * Asynchronously determine whether any Google accounts have been added.
     */
    public void hasGoogleAccounts(final Callback<Boolean> callback) {
        getGoogleAccounts(new Callback<Account[]>() {
            @Override
            public void onResult(Account[] accounts) {
                callback.onResult(accounts.length > 0);
            }
        });
    }

    private String canonicalizeName(String name) {
        String[] parts = AT_SYMBOL.split(name);
        if (parts.length != 2) return name;

        if (GOOGLEMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[1] = GMAIL_COM;
        }
        if (GMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[0] = parts[0].replace(".", "");
        }
        return (parts[0] + "@" + parts[1]).toLowerCase(Locale.US);
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public Account getAccountFromName(String accountName) {
        String canonicalName = canonicalizeName(accountName);
        Account[] accounts = getGoogleAccounts();
        for (Account account : accounts) {
            if (canonicalizeName(account.name).equals(canonicalName)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Asynchronously returns the account if it exists; null otherwise.
     */
    public void getAccountFromName(String accountName, final Callback<Account> callback) {
        final String canonicalName = canonicalizeName(accountName);
        getGoogleAccounts(new Callback<Account[]>() {
            @Override
            public void onResult(Account[] accounts) {
                Account accountForName = null;
                for (Account account : accounts) {
                    if (canonicalizeName(account.name).equals(canonicalName)) {
                        accountForName = account;
                        break;
                    }
                }
                callback.onResult(accountForName);
            }
        });
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public boolean hasAccountForName(String accountName) {
        return getAccountFromName(accountName) != null;
    }

    /**
     * Asynchronously returns whether an account exists with the given name.
     */
    // TODO(maxbogue): Remove once this function is used outside of tests.
    @VisibleForTesting
    public void hasAccountForName(String accountName, final Callback<Boolean> callback) {
        getAccountFromName(accountName, new Callback<Account>() {
            @Override
            public void onResult(Account account) {
                callback.onResult(account != null);
            }
        });
    }

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    public boolean hasGoogleAccountAuthenticator() {
        AuthenticatorDescription[] descs = mAccountManager.getAuthenticatorTypes();
        for (AuthenticatorDescription desc : descs) {
            if (GOOGLE_ACCOUNT_TYPE.equals(desc.type)) return true;
        }
        return false;
    }

    /**
     * Gets the auth token and returns the response asynchronously.
     * This should be called when we have a foreground activity that needs an auth token.
     * If encountered an IO error, it will attempt to retry when the network is back.
     *
     * - Assumes that the account is a valid account.
     */
    public void getAuthToken(final Account account, final String authTokenType,
            final GetAuthTokenCallback callback) {
        ConnectionRetry.runAuthTask(new AuthTask<String>() {
            @Override
            public String run() throws AuthException {
                return mAccountManager.getAuthToken(account, authTokenType);
            }
            @Override
            public void onSuccess(String token) {
                callback.tokenAvailable(token);
            }
            @Override
            public void onFailure(boolean isTransientError) {
                callback.tokenUnavailable(isTransientError);
            }
        });
    }

    public boolean hasGetAccountsPermission() {
        return mApplicationContext.checkPermission(
                       Manifest.permission.GET_ACCOUNTS, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Invalidates the old token (if non-null/non-empty) and asynchronously generates a new one.
     *
     * - Assumes that the account is a valid account.
     */
    public void getNewAuthToken(Account account, String authToken, String authTokenType,
            GetAuthTokenCallback callback) {
        invalidateAuthToken(authToken);
        getAuthToken(account, authTokenType, callback);
    }

    /**
     * Clear an auth token from the local cache with respect to the ApplicationContext.
     */
    public void invalidateAuthToken(final String authToken) {
        if (authToken == null || authToken.isEmpty()) {
            return;
        }
        ConnectionRetry.runAuthTask(new AuthTask<Boolean>() {
            @Override
            public Boolean run() throws AuthException {
                mAccountManager.invalidateAuthToken(authToken);
                return true;
            }
            @Override
            public void onSuccess(Boolean result) {}
            @Override
            public void onFailure(boolean isTransientError) {
                Log.e(TAG, "Failed to invalidate auth token: " + authToken);
            }
        });
    }

    public void checkChildAccount(Account account, Callback<Boolean> callback) {
        String[] features = {FEATURE_IS_CHILD_ACCOUNT_KEY};
        mAccountManager.hasFeatures(account, features, callback);
    }

    private interface AuthTask<T> {
        T run() throws AuthException;
        void onSuccess(T result);
        void onFailure(boolean isTransientError);
    }

    /**
     * A helper class to encapsulate network connection retry logic for AuthTasks.
     *
     * The task will be run on the background thread. If it encounters a transient error, it will
     * wait for a network change and retry up to MAX_TRIES times.
     */
    private static class ConnectionRetry<T>
            implements NetworkChangeNotifier.ConnectionTypeObserver {
        private static final int MAX_TRIES = 3;

        private final AuthTask<T> mAuthTask;
        private final AtomicInteger mNumTries;
        private final AtomicBoolean mIsTransientError;

        public static <T> void runAuthTask(AuthTask<T> authTask) {
            new ConnectionRetry<T>(authTask).attempt();
        }

        private ConnectionRetry(AuthTask<T> authTask) {
            mAuthTask = authTask;
            mNumTries = new AtomicInteger(0);
            mIsTransientError = new AtomicBoolean(false);
        }

        /**
         * Tries running the {@link AuthTask} in the background. This object is never registered
         * as a {@link ConnectionTypeObserver} when this method is called.
         */
        private void attempt() {
            // Clear any transient error.
            mIsTransientError.set(false);
            new AsyncTask<Void, Void, T>() {
                @Override
                public T doInBackground(Void... params) {
                    try {
                        return mAuthTask.run();
                    } catch (AuthException ex) {
                        Log.w(TAG, "Failed to perform auth task", ex);
                        mIsTransientError.set(ex.isTransientError());
                    }
                    return null;
                }
                @Override
                public void onPostExecute(T result) {
                    if (result != null) {
                        mAuthTask.onSuccess(result);
                    } else if (!mIsTransientError.get() || mNumTries.incrementAndGet() >= MAX_TRIES
                            || !NetworkChangeNotifier.isInitialized()) {
                        // Permanent error, ran out of tries, or we can't listen for network
                        // change events; give up.
                        mAuthTask.onFailure(mIsTransientError.get());
                    } else {
                        // Transient error with tries left; register for another attempt.
                        NetworkChangeNotifier.addConnectionTypeObserver(ConnectionRetry.this);
                    }
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        @Override
        public void onConnectionTypeChanged(int connectionType) {
            assert mNumTries.get() < MAX_TRIES;
            if (NetworkChangeNotifier.isOnline()) {
                // The network is back; stop listening and try again.
                NetworkChangeNotifier.removeConnectionTypeObserver(this);
                attempt();
            }
        }
    }
}
