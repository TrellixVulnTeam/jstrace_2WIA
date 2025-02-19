// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
WebInspector.View = function()
{
}

WebInspector.View.prototype = {
    /**
     * @return {string}
     */
    viewId: function() { },

    /**
     * @return {string}
     */
    title: function() { },

    /**
     * @return {boolean}
     */
    isCloseable: function() { },

    /**
     * @return {boolean}
     */
    isTransient: function() { },

    /**
     * @return {!Promise<!Array<!WebInspector.ToolbarItem>>}
     */
    toolbarItems: function() { },

    /**
     * @return {!Promise<!WebInspector.Widget>}
     */
    widget: function() { }
}

/**
 * @constructor
 * @extends {WebInspector.VBox}
 * @implements {WebInspector.View}
 * @param {string} title
 * @param {boolean=} isWebComponent
 */
WebInspector.SimpleView = function(title, isWebComponent)
{
    WebInspector.VBox.call(this, isWebComponent);
    this._title = title;
    /** @type {!Array<!WebInspector.ToolbarItem>} */
    this._toolbarItems = [];
}

WebInspector.SimpleView.prototype = {
    /**
     * @override
     * @return {string}
     */
    viewId: function()
    {
        return this._title;
    },

    /**
     * @override
     * @return {string}
     */
    title: function()
    {
        return this._title;
    },

    /**
     * @override
     * @return {boolean}
     */
    isCloseable: function()
    {
        return false;
    },

    /**
     * @override
     * @return {boolean}
     */
    isTransient: function()
    {
        return false;
    },

    /**
     * @override
     * @return {!Promise<!Array<!WebInspector.ToolbarItem>>}
     */
    toolbarItems: function()
    {
        return Promise.resolve(this.syncToolbarItems());
    },

    /**
     * @return {!Array<!WebInspector.ToolbarItem>}
     */
    syncToolbarItems: function()
    {
        return this._toolbarItems;
    },

    /**
     * @override
     * @return {!Promise<!WebInspector.Widget>}
     */
    widget: function()
    {
        return /** @type {!Promise<!WebInspector.Widget>} */ (Promise.resolve(this));
    },

    /**
     * @param {!WebInspector.ToolbarItem} item
     */
    addToolbarItem: function(item)
    {
        this._toolbarItems.push(item);
    },

    /**
     * @return {!Promise}
     */
    revealView: function()
    {
        return WebInspector.viewManager.revealView(this._parentViewToReveal || this);
    },

    /**
     * @param {!WebInspector.View} view
     */
    setParentViewForReveal: function(view)
    {
        this._parentViewToReveal = view;
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @implements {WebInspector.View}
 * @param {!Runtime.Extension} extension
 */
WebInspector.ProvidedView = function(extension)
{
    this._extension = extension;
}

WebInspector.ProvidedView.prototype = {
    /**
     * @override
     * @return {string}
     */
    viewId: function()
    {
        return this._extension.descriptor()["id"];
    },

    /**
     * @override
     * @return {string}
     */
    title: function()
    {
        return this._extension.title();
    },

    /**
     * @override
     * @return {boolean}
     */
    isCloseable: function()
    {
        return this._extension.descriptor()["persistence"] === "closeable";
    },

    /**
     * @override
     * @return {boolean}
     */
    isTransient: function()
    {
        return this._extension.descriptor()["persistence"] === "transient";
    },

    /**
     * @override
     * @return {!Promise<!Array<!WebInspector.ToolbarItem>>}
     */
    toolbarItems: function()
    {
        return Promise.resolve([]);
    },

    /**
     * @override
     * @return {!Promise<!WebInspector.Widget>}
     */
    widget: function()
    {
        return  /** @type {!Promise<!WebInspector.Widget>} */ (this._extension.instance());
    }
}

/**
 * @interface
 */
WebInspector.ViewLocation = function() { }

WebInspector.ViewLocation.prototype = {
    /**
     * @param {!WebInspector.View} view
     * @param {?WebInspector.View=} insertBefore
     */
    appendView: function(view, insertBefore) { },

    /**
     * @param {!WebInspector.View} view
     * @param {?WebInspector.View=} insertBefore
     * @return {!Promise}
     */
    showView: function(view, insertBefore) { },

    /**
     * @return {!WebInspector.Widget}
     */
    widget: function() { }
}

/**
 * @interface
 * @extends {WebInspector.ViewLocation}
 */
WebInspector.TabbedViewLocation = function() { }

WebInspector.TabbedViewLocation.prototype = {
    /**
     * @return {!WebInspector.TabbedPane}
     */
    tabbedPane: function() { },

    enableMoreTabsButton: function() { }
}

/**
 * @interface
 */
WebInspector.ViewLocationResolver = function() { }

WebInspector.ViewLocationResolver.prototype = {
    /**
     * @param {string} location
     * @return {?WebInspector.ViewLocation}
     */
    resolveLocation: function(location) { }
}

/**
 * @constructor
 */
WebInspector.ViewManager = function()
{
    /** @type {!Map<string, !WebInspector.View>} */
    this._views = new Map();
    /** @type {!Map<string, string>} */
    this._locationNameByViewId = new Map();

    for (var extension of self.runtime.extensions("view")) {
        var descriptor = extension.descriptor();
        this._views.set(descriptor["id"], new WebInspector.ProvidedView(extension));
        this._locationNameByViewId.set(descriptor["id"], descriptor["location"]);
    }
}

WebInspector.ViewManager.prototype = {
    /**
     * @param {!WebInspector.View} view
     * @return {!Promise}
     */
    revealView: function(view)
    {
        var location = /** @type {?WebInspector.ViewManager._Location} */ (view[WebInspector.ViewManager._Location.symbol]);
        if (!location)
            return Promise.resolve();
        location._reveal();
        return location.showView(view);
    },

    /**
     * @param {string} viewId
     * @return {!Promise}
     */
    showView: function(viewId)
    {
        var view = this._views.get(viewId);
        if (!view) {
            console.error("Could not find view for id: '" + viewId + "' " + new Error().stack);
            return Promise.resolve();
        }
        var locationName = this._locationNameByViewId.get(viewId);
        if (locationName === "drawer-view")
            WebInspector.userMetrics.drawerShown(viewId);

        return this._resolveLocation(locationName).then(location => {
            if (!location)
                return;
            location._reveal();
            return location.showView(view);
        });
    },

    /**
     * @param {string=} location
     * @return {!Promise<?WebInspector.ViewManager._Location>}
     */
    _resolveLocation: function(location)
    {
        if (!location)
            return /** @type {!Promise<?WebInspector.ViewManager._Location>} */ (Promise.resolve(null));

        var resolverExtensions = self.runtime.extensions(WebInspector.ViewLocationResolver).filter(extension => extension.descriptor()["name"] === location);
        if (!resolverExtensions.length)
            return /** @type {!Promise<?WebInspector.ViewManager._Location>} */ (Promise.resolve(null));
        var resolverExtension = resolverExtensions[0];
        return resolverExtension.instance().then(resolver => /** @type {?WebInspector.ViewManager._Location} */(resolver.resolveLocation(location)));
    },

    /**
     * @param {function()=} revealCallback
     * @param {string=} location
     * @param {boolean=} restoreSelection
     * @return {!WebInspector.TabbedViewLocation}
     */
    createTabbedLocation: function(revealCallback, location, restoreSelection)
    {
        return new WebInspector.ViewManager._TabbedLocation(this, revealCallback, location, restoreSelection);
    },

    /**
     * @param {function()=} revealCallback
     * @param {string=} location
     * @return {!WebInspector.ViewLocation}
     */
    createStackLocation: function(revealCallback, location)
    {
        return new WebInspector.ViewManager._StackLocation(this, revealCallback, location);
    },

    /**
     * @param {string} location
     * @return {!Array<!WebInspector.View>}
     */
    _viewsForLocation: function(location)
    {
        var result = [];
        for (var id of this._views.keys()) {
            if (this._locationNameByViewId.get(id) === location)
                result.push(this._views.get(id));
        }
        return result;
    }
}


/**
 * @param {!Element} element
 * @param {!Array<!WebInspector.ToolbarItem>} toolbarItems
 */
WebInspector.ViewManager._populateToolbar = function(element, toolbarItems)
{
    if (!toolbarItems.length)
        return;
    var toolbar = new WebInspector.Toolbar("");
    element.insertBefore(toolbar.element, element.firstChild);
    for (var item of toolbarItems)
        toolbar.appendToolbarItem(item);
}

/**
 * @constructor
 * @extends {WebInspector.VBox}
 * @param {!WebInspector.View} view
 */
WebInspector.ViewManager._ContainerWidget = function(view)
{
    WebInspector.VBox.call(this);
    this.element.classList.add("flex-auto", "view-container", "overflow-auto");
    this._view = view;
}

WebInspector.ViewManager._ContainerWidget.prototype = {
    /**
     * @return {!Promise}
     */
    _materialize: function()
    {
        if (this._materializePromise)
            return this._materializePromise;
        var promises = [];
        promises.push(this._view.toolbarItems().then(WebInspector.ViewManager._populateToolbar.bind(WebInspector.ViewManager, this.element)));
        promises.push(this._view.widget().then(widget => widget.show(this.element)));
        this._materializePromise = Promise.all(promises);
        return this._materializePromise;
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @extends {WebInspector.VBox}
 * @param {!WebInspector.View} view
 */
WebInspector.ViewManager._ExpandableContainerWidget = function(view)
{
    WebInspector.VBox.call(this, true);
    this.element.classList.add("flex-none");
    this.registerRequiredCSS("ui/viewContainers.css");

    this._titleElement = createElementWithClass("div", "expandable-view-title");
    this._titleElement.textContent = view.title();
    this._titleElement.tabIndex = 0;
    this._titleElement.addEventListener("click", this._toggleExpanded.bind(this), false);
    this._titleElement.addEventListener("keydown", this._onTitleKeyDown.bind(this), false);
    this.contentElement.insertBefore(this._titleElement, this.contentElement.firstChild);

    this.contentElement.createChild("content");
    this._view = view;
    view[WebInspector.ViewManager._ExpandableContainerWidget._symbol] = this;
}

WebInspector.ViewManager._ExpandableContainerWidget._symbol = Symbol("container");

WebInspector.ViewManager._ExpandableContainerWidget.prototype = {
    /**
     * @return {!Promise}
     */
    _materialize: function()
    {
        if (this._materializePromise)
            return this._materializePromise;
        var promises = [];
        promises.push(this._view.toolbarItems().then(WebInspector.ViewManager._populateToolbar.bind(WebInspector.ViewManager, this._titleElement)));
        promises.push(this._view.widget().then(widget => {
            this._widget = widget;
            widget.show(this.element);
        }));
        this._materializePromise = Promise.all(promises);
        return this._materializePromise;
    },

    /**
     * @return {!Promise}
     */
    _expand: function()
    {
        if (this._titleElement.classList.contains("expanded"))
            return this._materialize();
        this._titleElement.classList.add("expanded");
        return this._materialize().then(() => this._widget.show(this.element));
    },

    _collapse: function()
    {
        if (!this._titleElement.classList.contains("expanded"))
            return;
        this._titleElement.classList.remove("expanded");
        this._materialize().then(() => this._widget.detach());
    },

    _toggleExpanded: function()
    {
        if (this._titleElement.classList.contains("expanded"))
            this._collapse();
        else
            this._expand();
    },

    /**
     * @param {!Event} event
     */
    _onTitleKeyDown: function(event)
    {
        if (isEnterKey(event) || event.keyCode === WebInspector.KeyboardShortcut.Keys.Space.code)
            this._toggleExpanded();
    },

    __proto__: WebInspector.VBox.prototype
}

/**
 * @constructor
 * @param {!WebInspector.ViewManager} manager
 * @param {!WebInspector.Widget} widget
 * @param {function()=} revealCallback
 * @param {string=} location
 */
WebInspector.ViewManager._Location = function(manager, widget, revealCallback, location)
{
    this._manager = manager;
    this._revealCallback = revealCallback;
    this._location = location;
    this._widget = widget;
}

WebInspector.ViewManager._Location.symbol = Symbol("location");

WebInspector.ViewManager._Location.prototype = {
    /**
     * @return {!WebInspector.Widget}
     */
    widget: function()
    {
        return this._widget;
    },

    _reveal: function()
    {
        if (this._revealCallback)
            this._revealCallback();
    }
}

/**
 * @constructor
 * @extends {WebInspector.ViewManager._Location}
 * @implements {WebInspector.TabbedViewLocation}
 * @param {!WebInspector.ViewManager} manager
 * @param {function()=} revealCallback
 * @param {string=} location
 * @param {boolean=} restoreSelection
 */
WebInspector.ViewManager._TabbedLocation = function(manager, revealCallback, location, restoreSelection)
{
    this._tabbedPane = new WebInspector.TabbedPane();
    WebInspector.ViewManager._Location.call(this, manager, this._tabbedPane, revealCallback, location);

    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected, this._tabSelected, this);
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabClosed, this._tabClosed, this);
    this._closeableTabSetting = WebInspector.settings.createSetting(location + "-closeableTabs", {});
    if (restoreSelection)
        this._lastSelectedTabSetting = WebInspector.settings.createSetting(location + "-selectedTab", "");

    /** @type {!Map.<string, !WebInspector.View>} */
    this._views = new Map();
    this._populateLocation();
}

WebInspector.ViewManager._TabbedLocation.prototype = {
    /**
     * @override
     * @return {!WebInspector.Widget}
     */
    widget: function()
    {
        return this._tabbedPane;
    },

    /**
     * @override
     * @return {!WebInspector.TabbedPane}
     */
    tabbedPane: function()
    {
        return this._tabbedPane;
    },

    /**
     * @override
     */
    enableMoreTabsButton: function()
    {
        var toolbar = new WebInspector.Toolbar("drawer-toolbar");
        toolbar.appendToolbarItem(new WebInspector.ToolbarMenuButton(this._appendTabsToMenu.bind(this)));
        this._tabbedPane.insertBeforeTabStrip(toolbar.element);
        this._tabbedPane.disableOverflowMenu();
    },

    _populateLocation: function()
    {
        if (!this._location)
            return;
        for (var view of this._manager._viewsForLocation(this._location)) {
            var id = view.viewId();
            this._views.set(id, view);
            view[WebInspector.ViewManager._Location.symbol] = this;
            if (view.isTransient())
                continue;
            if (!view.isCloseable())
                this._appendTab(view);
            else if (this._closeableTabSetting.get()[id])
                this._appendTab(view);
        }
    },

    wasShown: function()
    {
        if (this._wasAlreadyShown || !this._lastSelectedTabSetting)
            return;
        this._wasAlreadyShown = true;
        if (this._tabbedPane.hasTab(this._lastSelectedTabSetting.get()))
            this._tabbedPane.selectTab(this._lastSelectedTabSetting.get());
    },

    /**
     * @param {!WebInspector.ContextMenu} contextMenu
     */
    _appendTabsToMenu: function(contextMenu)
    {
        for (var view of this._views.values()) {
            var title = WebInspector.UIString(view.title());
            contextMenu.appendItem(title, this.showView.bind(this, view));
        }
    },

    /**
     * @param {!WebInspector.View} view
     */
    _appendTab: function(view)
    {
        this._tabbedPane.appendTab(view.viewId(), view.title(), new WebInspector.ViewManager._ContainerWidget(view), undefined, false, view.isCloseable() || view.isTransient());
    },

    /**
     * @override
     * @param {!WebInspector.View} view
     * @param {?WebInspector.View=} insertBefore
     */
    appendView: function(view, insertBefore)
    {
        if (insertBefore)
            throw new Error("Insert before in tabbed pane is not supported");
        if (!this._tabbedPane.hasTab(view.viewId())) {
            view[WebInspector.ViewManager._Location.symbol] = this;
            this._views.set(view.viewId(), view);
            this._appendTab(view);
        }
    },

    /**
     * @override
     * @param {!WebInspector.View} view
     * @param {?WebInspector.View=} insertBefore
     * @return {!Promise}
     */
    showView: function(view, insertBefore)
    {
        this.appendView(view, insertBefore);
        this._tabbedPane.focus();
        this._tabbedPane.selectTab(view.viewId());
        return this._materializeWidget(view);
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _tabSelected: function(event)
    {
        var tabId = /** @type {string} */ (event.data.tabId);
        if (this._lastSelectedTabSetting && event.data["isUserGesture"])
            this._lastSelectedTabSetting.set(tabId);
        var view = this._views.get(tabId);
        if (!view)
            return;

        this._materializeWidget(view);

        if (view.isCloseable()) {
            var tabs = this._closeableTabSetting.get();
            if (!tabs[tabId]) {
                tabs[tabId] = true;
                this._closeableTabSetting.set(tabs);
            }
        }
    },

    /**
     * @param {!WebInspector.Event} event
     */
    _tabClosed: function(event)
    {
        var id = /** @type {string} */ (event.data["tabId"]);
        var tabs = this._closeableTabSetting.get();
        if (tabs[id]) {
            delete tabs[id];
            this._closeableTabSetting.set(tabs);
        }
    },

    /**
     * @param {!WebInspector.View} view
     * @return {!Promise}
     */
    _materializeWidget: function(view)
    {
        var widget = /** @type {!WebInspector.ViewManager._ContainerWidget} */ (this._tabbedPane.tabView(view.viewId()));
        return widget._materialize();
    },

    __proto__: WebInspector.ViewManager._Location.prototype
}

/**
 * @constructor
 * @extends {WebInspector.ViewManager._Location}
 * @implements {WebInspector.ViewLocation}
 * @param {!WebInspector.ViewManager} manager
 * @param {function()=} revealCallback
 * @param {string=} location
 */
WebInspector.ViewManager._StackLocation = function(manager, revealCallback, location)
{
    this._vbox = new WebInspector.VBox();
    WebInspector.ViewManager._Location.call(this, manager, this._vbox, revealCallback, location);

    /** @type {!Map<string, !WebInspector.ViewManager._ExpandableContainerWidget>} */
    this._expandableContainers = new Map();
    this._populateLocation();
}

WebInspector.ViewManager._StackLocation.prototype = {

    /**
     * @override
     * @param {!WebInspector.View} view
     * @param {?WebInspector.View=} insertBefore
     */
    appendView: function(view, insertBefore)
    {
        var container = this._expandableContainers.get(view.viewId());
        if (!container) {
            view[WebInspector.ViewManager._Location.symbol] = this;
            container = new WebInspector.ViewManager._ExpandableContainerWidget(view);
            var beforeElement = null;
            if (insertBefore) {
                var beforeContainer = insertBefore[WebInspector.ViewManager._ExpandableContainerWidget._symbol];
                beforeElement = beforeContainer ? beforeContainer.element : null;
            }
            container.show(this._vbox.contentElement, beforeElement);
            this._expandableContainers.set(view.viewId(), container);
        }
    },

    /**
     * @override
     * @param {!WebInspector.View} view
     * @param {?WebInspector.View=} insertBefore
     * @return {!Promise}
     */
    showView: function(view, insertBefore)
    {
        this.appendView(view, insertBefore);
        var container = this._expandableContainers.get(view.viewId());
        return container._expand();
    },

    _populateLocation: function()
    {
        if (!this._location)
            return;
        for (var view of this._manager._viewsForLocation(this._location))
            this.appendView(view);
    },

    __proto__: WebInspector.ViewManager._Location.prototype
}

/**
 * @type {!WebInspector.ViewManager}
 */
WebInspector.viewManager;
