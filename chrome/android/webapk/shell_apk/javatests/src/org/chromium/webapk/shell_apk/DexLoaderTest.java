// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.FileObserver;
import android.os.IBinder;
import android.os.RemoteException;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import dalvik.system.DexFile;

import org.chromium.base.FileUtils;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.webapk.shell_apk.test.dex_optimizer.IDexOptimizerService;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests for {@link DexLoader}.
 */
public class DexLoaderTest extends InstrumentationTestCase {
    /**
     * Package of APK to load dex file from and package which provides DexOptimizerService.
     */
    private static final String DEX_OPTIMIZER_SERVICE_PACKAGE =
            "org.chromium.webapk.shell_apk.test.dex_optimizer";

    /**
     * Class which implements DexOptimizerService.
     */
    private static final String DEX_OPTIMIZER_SERVICE_CLASS_NAME =
            "org.chromium.webapk.shell_apk.test.dex_optimizer.DexOptimizerServiceImpl";

    /**
     * Name of dex files in DexOptimizer.apk.
     */
    private static final String DEX_ASSET_NAME = "canary.dex";
    private static final String DEX_ASSET_NAME2 = "canary2.dex";

    /**
     * Classes to load to check whether dex is valid.
     */
    private static final String CANARY_CLASS_NAME =
            "org.chromium.webapk.shell_apk.test.canary.Canary";
    private static final String CANARY_CLASS_NAME2 =
            "org.chromium.webapk.shell_apk.test.canary.Canary2";

    private Context mContext;
    private Context mRemoteContext;
    private DexLoader mDexLoader;
    private File mLocalDexDir;
    private IDexOptimizerService mDexOptimizerService;
    private ServiceConnection mServiceConnection;

    /**
     * Monitors read files and modified files in the directory passed to the constructor.
     */
    private static class FileMonitor extends FileObserver {
        public ArrayList<String> mReadPaths = new ArrayList<String>();
        public ArrayList<String> mModifiedPaths = new ArrayList<String>();

        public FileMonitor(File directory) {
            super(directory.getPath());
        }

        @Override
        public void onEvent(int event, String path) {
            switch (event) {
                case FileObserver.ACCESS:
                    mReadPaths.add(path);
                    break;
                case FileObserver.CREATE:
                case FileObserver.DELETE:
                case FileObserver.DELETE_SELF:
                case FileObserver.MODIFY:
                    mModifiedPaths.add(path);
                    break;
                default:
                    break;
            }
        }
    }

    @Override
    protected void setUp() {
        mContext = getInstrumentation().getTargetContext();
        mRemoteContext = getRemoteContext(mContext);
        mDexLoader = new DexLoader();

        mLocalDexDir = mContext.getDir("dex", Context.MODE_PRIVATE);
        if (mLocalDexDir.exists()) {
            FileUtils.recursivelyDeleteFile(mLocalDexDir);
            if (mLocalDexDir.exists()) {
                fail("Could not delete local dex directory.");
            }
        }

        connectToDexOptimizerService();

        try {
            if (!mDexOptimizerService.deleteDexDirectory()) {
                fail("Could not delete remote dex directory.");
            }
        } catch (RemoteException e) {
            e.printStackTrace();
            fail("Remote crashed during setup.");
        }
    }

    @Override
    public void tearDown() throws Exception {
        mContext.unbindService(mServiceConnection);
        super.tearDown();
    }

    /**
     * Test that {@DexLoader#load()} can create a ClassLoader from a dex and optimized dex in
     * another app's data directory.
     */
    @MediumTest
    public void testLoadFromRemoteDataDir() {
        // Extract the dex file into another app's data directory and optimize the dex.
        String remoteDexFilePath = null;
        try {
            remoteDexFilePath = mDexOptimizerService.extractAndOptimizeDex();
        } catch (RemoteException e) {
            e.printStackTrace();
            fail("Remote crashed.");
        }

        if (remoteDexFilePath == null) {
            fail("Could not extract and optimize dex.");
        }

        // Check that the Android OS knows about the optimized dex file for
        // {@link remoteDexFilePath}.
        File remoteDexFile = new File(remoteDexFilePath);
        assertFalse(isDexOptNeeded(remoteDexFile));

        ClassLoader loader = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, remoteDexFile, mLocalDexDir);
        assertNotNull(loader);
        assertTrue(canLoadCanaryClass(loader));

        // Check that {@link DexLoader#load()} did not use the fallback path.
        assertFalse(mLocalDexDir.exists());
    }

    /**
     * That that {@link DexLoader#load()} falls back to extracting the dex from the APK to the
     * local data directory and creating the ClassLoader from the extracted dex if creating the
     * ClassLoader from the cached data in the remote Context's data directory fails.
     */
    @MediumTest
    public void testLoadFromLocalDataDir() {
        ClassLoader loader = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
        assertNotNull(loader);
        assertTrue(canLoadCanaryClass(loader));

        // Check that the dex file was extracted to the local data directory and that a directory
        // was created for the optimized dex.
        assertTrue(mLocalDexDir.exists());
        File[] localDexDirFiles = mLocalDexDir.listFiles();
        assertNotNull(localDexDirFiles);
        Arrays.sort(localDexDirFiles);
        assertEquals(2, localDexDirFiles.length);
        assertEquals(DEX_ASSET_NAME, localDexDirFiles[0].getName());
        assertFalse(localDexDirFiles[0].isDirectory());
        assertEquals("optimized", localDexDirFiles[1].getName());
        assertTrue(localDexDirFiles[1].isDirectory());
    }

    /**
     * Test that {@link DexLoader#load()} does not extract the dex file from the APK if the dex file
     * was extracted in a previous call to {@link DexLoader#load()}
     */
    @MediumTest
    public void testPreviouslyLoadedFromLocalDataDir() {
        assertTrue(mLocalDexDir.mkdir());

        {
            // Load dex the first time. This should extract the dex file from the APK's assets and
            // generate the optimized dex file.
            FileMonitor localDexDirMonitor = new FileMonitor(mLocalDexDir);
            localDexDirMonitor.startWatching();
            ClassLoader loader = mDexLoader.load(
                    mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
            localDexDirMonitor.stopWatching();

            assertNotNull(loader);
            assertTrue(canLoadCanaryClass(loader));

            assertTrue(localDexDirMonitor.mReadPaths.contains(DEX_ASSET_NAME));
            assertTrue(localDexDirMonitor.mModifiedPaths.contains(DEX_ASSET_NAME));
        }
        {
            // Load dex a second time. We should use the already extracted dex file.
            FileMonitor localDexDirMonitor = new FileMonitor(mLocalDexDir);
            localDexDirMonitor.startWatching();
            ClassLoader loader = mDexLoader.load(
                    mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
            localDexDirMonitor.stopWatching();

            // The returned ClassLoader should be valid.
            assertNotNull(loader);
            assertTrue(canLoadCanaryClass(loader));

            // We should not have modified any files.
            assertTrue(localDexDirMonitor.mModifiedPaths.isEmpty());
        }
    }

    /**
     * Test loading a dex file from a directory which was previously used for loading a different
     * dex file.
     */
    @MediumTest
    public void testLoadDifferentDexInLocalDataDir() {
        assertTrue(mLocalDexDir.mkdir());

        // Load canary.dex
        ClassLoader loader1 = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
        assertNotNull(loader1);
        assertTrue(canLoadCanaryClass(loader1));

        File canaryDexFile1 = new File(mLocalDexDir, DEX_ASSET_NAME);
        assertTrue(canaryDexFile1.exists());

        mDexLoader.deleteCachedDexes(mLocalDexDir);

        ClassLoader loader2 = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME2, CANARY_CLASS_NAME2, null, mLocalDexDir);
        assertNotNull(loader2);
        assertTrue(canLoadClass(loader2, CANARY_CLASS_NAME2));

        // canary2.dex should have been extracted and the previously extracted canary.dex file
        // should have been deleted.
        assertTrue(new File(mLocalDexDir, DEX_ASSET_NAME2).exists());
        assertFalse(canaryDexFile1.exists());
    }

    /**
     * Connects to the DexOptimizerService.
     */
    private void connectToDexOptimizerService() {
        Intent intent = new Intent();
        intent.setComponent(
                new ComponentName(DEX_OPTIMIZER_SERVICE_PACKAGE, DEX_OPTIMIZER_SERVICE_CLASS_NAME));
        final CallbackHelper connectedCallback = new CallbackHelper();

        mServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mDexOptimizerService = IDexOptimizerService.Stub.asInterface(service);
                connectedCallback.notifyCalled();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        try {
            mContext.bindService(intent, mServiceConnection, Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            e.printStackTrace();
            fail();
        }

        try {
            connectedCallback.waitForCallback(0);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Could not connect to remote.");
        }
    }

    /**
     * Returns the Context of the APK which provides DexOptimizerService.
     * @param context The test application's Context.
     * @return Context of the APK whcih provide DexOptimizerService.
     */
    private Context getRemoteContext(Context context) {
        try {
            return context.getApplicationContext().createPackageContext(
                    DEX_OPTIMIZER_SERVICE_PACKAGE,
                    Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
            fail("Could not get remote context");
            return null;
        }
    }

    /** Returns whether the Android OS thinks that a dex file needs to be re-optimized */
    private boolean isDexOptNeeded(File dexFile) {
        try {
            return DexFile.isDexOptNeeded(dexFile.getPath());
        } catch (Exception e) {
            e.printStackTrace();
            fail();
            return false;
        }
    }

    /** Returns whether the ClassLoader can load {@link CANARY_CLASS_NAME} */
    private boolean canLoadCanaryClass(ClassLoader loader) {
        return canLoadClass(loader, CANARY_CLASS_NAME);
    }

    /** Returns whether the ClassLoader can load a class */
    private boolean canLoadClass(ClassLoader loader, String className) {
        try {
            loader.loadClass(className);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
