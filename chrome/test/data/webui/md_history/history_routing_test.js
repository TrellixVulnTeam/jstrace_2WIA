// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_routing_test', function() {
  function registerTests() {
    suite('routing-test', function() {
      var app;
      var list;
      var toolbar;

      suiteSetup(function() {
        app = $('history-app');
        sidebar = app.$['content-side-bar']
        toolbar = app.$['toolbar'];
      });

      test('changing route changes active view', function() {
        assertEquals('history', app.$.content.selected);
        app.set('routeData_.page', 'syncedTabs');
        return flush().then(function() {
          assertEquals('syncedTabs', app.$.content.selected);
          assertEquals('chrome://history/syncedTabs', window.location.href);
        });
      });

      test('route updates from sidebar', function() {
        var menu = sidebar.$.menu;
        assertEquals('', app.routeData_.page);
        assertEquals('chrome://history/', window.location.href);

        MockInteractions.tap(menu.children[1]);
        assertEquals('syncedTabs', app.routeData_.page);
        assertEquals('chrome://history/syncedTabs', window.location.href);

        MockInteractions.tap(menu.children[0]);
        assertEquals('', app.routeData_.page);
        assertEquals('chrome://history/', window.location.href);
      });

      test('route updates from search', function() {
        var searchTerm = 'McCree';
        assertEquals('', app.routeData_.page);
        toolbar.setSearchTerm(searchTerm);
        assertEquals(searchTerm, app.queryParams_.q);
      });

      test('search updates from route', function() {
        var searchTerm = 'Mei';
        assertEquals('history', app.$.content.selected);
        app.set('queryParams_.q', searchTerm);
        assertEquals(searchTerm, toolbar.searchTerm);
      });

      test('search preserved across menu items', function() {
        var searchTerm = 'Soldier 76';
        var menu = sidebar.$.menu;
        assertEquals('', app.routeData_.page);
        assertEquals('history', app.$.content.selected);
        app.set('queryParams_.q', searchTerm);

        MockInteractions.tap(menu.children[1]);
        assertEquals('syncedTabs', app.routeData_.page);
        assertEquals(searchTerm, app.queryParams_.q);
        assertEquals(searchTerm, toolbar.searchTerm);

        MockInteractions.tap(menu.children[0]);
        assertEquals('', app.routeData_.page);
        assertEquals(searchTerm, app.queryParams_.q);
        assertEquals(searchTerm, toolbar.searchTerm);
      });

      teardown(function() {
        app.set('routeData_.page', '');
        app.set('queryParams_.q', null);
      });
    });
  }
  return {
    registerTests: registerTests
  };
});

cr.define('md_history.history_routing_test_with_query_param', function() {
  function registerTests() {
    suite('routing-with-query-param', function() {
      var app;
      var list;
      var toolbar;
      var expectedQuery;

      suiteSetup(function() {
        app = $('history-app');
        sidebar = app.$['side-bar']
        toolbar = app.$['toolbar'];
        expectedQuery = 'query';
      });

      test('search initiated on load', function(done) {
        var verifyFunction = function(info) {
          assertEquals(expectedQuery, info[0]);
          flush().then(function() {
            assertEquals(
                expectedQuery,
                toolbar.$['main-toolbar'].getSearchField().getValue());
            done();
          });
        };

        if (window.historyQueryInfo) {
          verifyFunction(window.historyQueryInfo);
          return;
        }

        registerMessageCallback('queryHistory', this, verifyFunction);
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
