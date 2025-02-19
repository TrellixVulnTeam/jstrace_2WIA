// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Definitions for the Chromium extensions API used by ChromeVox.
 *
 * @externs
 */


// TODO: Move these to //third_party/closure_compiler/externs.

// Begin auto generated externs; do not edit.
// The following was generated from:
//
// python tools/json_schema_compiler/compiler.py
//     -g externs
//     chrome/common/extensions/api/automation.idl

/**
 * @const
 */
chrome.automation = {};

/**
 * @enum {string}
 */
chrome.automation.EventType = {
  activedescendantchanged: '',
  alert: '',
  ariaAttributeChanged: '',
  autocorrectionOccured: '',
  blur: '',
  checkedStateChanged: '',
  childrenChanged: '',
  documentSelectionChanged: '',
  focus: '',
  hide: '',
  hover: '',
  invalidStatusChanged: '',
  layoutComplete: '',
  liveRegionChanged: '',
  loadComplete: '',
  locationChanged: '',
  menuEnd: '',
  menuListItemSelected: '',
  menuListValueChanged: '',
  menuPopupEnd: '',
  menuPopupStart: '',
  menuStart: '',
  rowCollapsed: '',
  rowCountChanged: '',
  rowExpanded: '',
  scrollPositionChanged: '',
  scrolledToAnchor: '',
  selectedChildrenChanged: '',
  selection: '',
  selectionAdd: '',
  selectionRemove: '',
  show: '',
  textChanged: '',
  textSelectionChanged: '',
  treeChanged: '',
  valueChanged: '',
};

/**
 * @enum {string}
 */
chrome.automation.RoleType = {
  alertDialog: '',
  alert: '',
  annotation: '',
  application: '',
  article: '',
  banner: '',
  blockquote: '',
  busyIndicator: '',
  button: '',
  buttonDropDown: '',
  canvas: '',
  caption: '',
  cell: '',
  checkBox: '',
  client: '',
  colorWell: '',
  columnHeader: '',
  column: '',
  comboBox: '',
  complementary: '',
  contentInfo: '',
  date: '',
  dateTime: '',
  definition: '',
  descriptionListDetail: '',
  descriptionList: '',
  descriptionListTerm: '',
  desktop: '',
  details: '',
  dialog: '',
  directory: '',
  disclosureTriangle: '',
  div: '',
  document: '',
  embeddedObject: '',
  figcaption: '',
  figure: '',
  footer: '',
  form: '',
  grid: '',
  group: '',
  heading: '',
  iframe: '',
  iframePresentational: '',
  ignored: '',
  imageMapLink: '',
  imageMap: '',
  image: '',
  inlineTextBox: '',
  labelText: '',
  legend: '',
  lineBreak: '',
  link: '',
  listBoxOption: '',
  listBox: '',
  listItem: '',
  listMarker: '',
  list: '',
  locationBar: '',
  log: '',
  main: '',
  marquee: '',
  math: '',
  menuBar: '',
  menuButton: '',
  menuItem: '',
  menuItemCheckBox: '',
  menuItemRadio: '',
  menuListOption: '',
  menuListPopup: '',
  menu: '',
  meter: '',
  navigation: '',
  note: '',
  outline: '',
  pane: '',
  paragraph: '',
  popUpButton: '',
  pre: '',
  presentational: '',
  progressIndicator: '',
  radioButton: '',
  radioGroup: '',
  region: '',
  rootWebArea: '',
  rowHeader: '',
  row: '',
  ruby: '',
  ruler: '',
  svgRoot: '',
  scrollArea: '',
  scrollBar: '',
  seamlessWebArea: '',
  search: '',
  searchBox: '',
  slider: '',
  sliderThumb: '',
  spinButtonPart: '',
  spinButton: '',
  splitter: '',
  staticText: '',
  status: '',
  switch: '',
  tabGroup: '',
  tabList: '',
  tabPanel: '',
  tab: '',
  tableHeaderContainer: '',
  table: '',
  textField: '',
  time: '',
  timer: '',
  titleBar: '',
  toggleButton: '',
  toolbar: '',
  treeGrid: '',
  treeItem: '',
  tree: '',
  unknown: '',
  tooltip: '',
  webArea: '',
  webView: '',
  window: '',
};

/**
 * @enum {string}
 */
chrome.automation.StateType = {
  busy: '',
  checked: '',
  collapsed: '',
  default: '',
  disabled: '',
  editable: '',
  expanded: '',
  focusable: '',
  focused: '',
  haspopup: '',
  horizontal: '',
  hovered: '',
  indeterminate: '',
  invisible: '',
  linked: '',
  multiline: '',
  multiselectable: '',
  offscreen: '',
  pressed: '',
  protected: '',
  readOnly: '',
  required: '',
  richlyEditable: '',
  selectable: '',
  selected: '',
  vertical: '',
  visited: '',
};

/**
 * @enum {number}
 */
chrome.automation.NameFromType = {
  0: '',
  1: 'uninitialized',
  2: 'attribute',
  3: 'contents',
  4: 'placeholder',
  5: 'relatedElement',
  6: 'value'
};

/**
 * @enum {number}
 */
chrome.automation.DescriptionFromType = {
  0: '',
  1: 'uninitialized',
  2: 'attribute',
  3: 'contents',
  4: 'placeholder',
  5: 'relatedElement'
};

/**
 * @enum {string}
 */
chrome.automation.TreeChangeType = {
  nodeCreated: 'nodeCreated',
  subtreeCreated: 'subtreeCreated',
  nodeChanged: 'nodeChanged',
  nodeRemoved: 'nodeRemoved',
};

/**
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
chrome.automation.Rect;

/**
 * @typedef {{
 *   role: (!chrome.automation.RoleType|undefined),
 *   state: (Object|undefined),
 *   attributes: (Object|undefined)
 * }}
 */
chrome.automation.FindParams;

/**
 * @constructor
 * @param {chrome.automation.EventType} type
 * @param {chrome.automation.AutomationNode} node
 */
chrome.automation.AutomationEvent = function(type, node) {};

/**
 * @type {!chrome.automation.AutomationNode}
 */
chrome.automation.AutomationEvent.prototype.target;

/**
 * @type {!chrome.automation.EventType}
 */
chrome.automation.AutomationEvent.prototype.type;

chrome.automation.AutomationEvent.prototype.stopPropagation = function() {};

/**
 * @typedef {{
 *   target: chrome.automation.AutomationNode,
 *   type: !chrome.automation.TreeChangeType
 * }}
 */
chrome.automation.TreeChange;

/**
 * @constructor
 */
chrome.automation.AutomationNode = function() {};


/**
 * @param {number} tabId
 * @param {function(chrome.automation.AutomationNode):void} callback
 */
chrome.automation.getTree = function(tabId, callback) {};

/** @param {function(!chrome.automation.AutomationNode):void} callback */
chrome.automation.getDesktop = function(callback) {};

/** @param {function(!chrome.automation.AutomationNode):void} callback */
chrome.automation.getFocus = function(callback) {};

/**
 * @param {string} filter
 * @param {function(chrome.automation.TreeChange) : void}
 *    observer
 */
chrome.automation.addTreeChangeObserver = function(filter, observer) {};

/**
 * @param {function(chrome.automation.TreeChange) : void} observer
 */
chrome.automation.removeTreeChangeObserver = function(observer) {};

//
// End auto generated externs; do not edit.
//



/**
 * @type {chrome.automation.RoleType}
 */
chrome.automation.AutomationNode.prototype.role;


/**
 * @type {!Object<chrome.automation.StateType, boolean>}
 */
chrome.automation.AutomationNode.prototype.state;


/**
 * @type {chrome.automation.NameFromType}
 */
chrome.automation.AutomationNode.prototype.nameFrom;


/**
 * @type {chrome.automation.DescriptionFromType}
 */
chrome.automation.AutomationNode.prototype.descriptionFrom;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.indexInParent;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.name;

/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.description;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.url;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.docUrl;


/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.value;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.textSelStart;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.textSelEnd;


/**
 * @type {Array<number>}
 */
chrome.automation.AutomationNode.prototype.wordStarts;


/**
 * @type {Array<number>}
 */
chrome.automation.AutomationNode.prototype.wordEnds;


/**
 * @type {chrome.automation.AutomationRootNode}
 */
chrome.automation.AutomationNode.prototype.root;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.firstChild;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.lastChild;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.nextSibling;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.previousSibling;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.parent;


/**
 * @type {!Array<chrome.automation.AutomationNode>}
 */
chrome.automation.AutomationNode.prototype.children;


/**
 * @type {{top: number, left: number, height: number, width: number}|undefined}
 */
chrome.automation.AutomationNode.prototype.location;


/**
 * @param {number} start
 * @param {number} end
 * @return {
 *     ({top: number, left: number, height: number, width: number})|undefined}
 */
chrome.automation.AutomationNode.prototype.boundsForRange =
    function(start, end) {};


chrome.automation.AutomationNode.prototype.makeVisible = function() {};


/**
 * @param {chrome.automation.EventType} eventType
 * @param {function(!chrome.automation.AutomationEvent) : void} callback
 * @param {boolean} capture
 */
chrome.automation.AutomationNode.prototype.addEventListener =
    function(eventType, callback, capture) {};


/**
 * @param {chrome.automation.EventType} eventType
 * @param {function(!chrome.automation.AutomationEvent) : void} callback
 * @param {boolean} capture
 */
chrome.automation.AutomationNode.prototype.removeEventListener =
    function(eventType, callback, capture) {};


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.TreeChange.prototype.target;


/**
 * @type {chrome.automation.TreeChangeType}
 */
chrome.automation.TreeChange.prototype.type;


chrome.automation.AutomationNode.prototype.doDefault = function() {};


chrome.automation.AutomationNode.prototype.focus = function() {};


chrome.automation.AutomationNode.prototype.showContextMenu = function() {};


/**
 * @param {number} start
 * @param {number} end
 */
chrome.automation.AutomationNode.prototype.setSelection =
    function(start, end) {};

/** @type {string} */
chrome.automation.AutomationNode.prototype.containerLiveStatus;

/** @type {string} */
chrome.automation.AutomationNode.prototype.containerLiveRelevant;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.containerLiveAtomic;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.containerLiveBusy;

/** @type {string} */
chrome.automation.AutomationNode.prototype.language;

/** @type {string} */
chrome.automation.AutomationNode.prototype.liveStatus;

/** @type {string} */
chrome.automation.AutomationNode.prototype.liveRelevant;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.liveAtomic;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.liveBusy;


/**
 * @param {Object} findParams
 */
chrome.automation.AutomationNode.prototype.find = function(findParams) {};

/**
 * @param {Object} findParams
 * @return {Array<chrome.automation.AutomationNode>}
 */
chrome.automation.AutomationNode.prototype.findAll = function(findParams) {};

/**
 * @type {string}
 */
chrome.automation.AutomationNode.prototype.inputType;

/**
 * @type {(chrome.automation.AutomationNode|undefined)}
 */
chrome.automation.AutomationNode.prototype.anchorObject;

/**
 * @param {{anchorObject: !chrome.automation.AutomationNode,
 *          anchorOffset: number,
 *          focusObject: !chrome.automation.AutomationNode,
 *          focusOffset: number}} selectionParams
 */
chrome.automation.setDocumentSelection = function(selectionParams) {};

/**
 * @type {(number|undefined)}
 */
chrome.automation.anchorOffset;

/**
 * @type {(chrome.automation.AutomationNode|undefined)}
 */
chrome.automation.AutomationNode.prototype.focusObject;

/**
 * @type {(Array<number>|undefined)}
 */
chrome.automation.AutomationNode.prototype.lineBreaks;

/**
 * @type {(number|undefined)}
 */
chrome.automation.focusOffset;

/**
 * @type {(chrome.automation.AutomationNode|undefined)}
 */
chrome.automation.AutomationNode.prototype.activeDescendant;

/** @type {number} */
chrome.automation.AutomationNode.prototype.tableCellColumnIndex;

/** @type {number} */
chrome.automation.AutomationNode.prototype.tableCellRowIndex;

/** @type {number} */
chrome.automation.AutomationNode.prototype.tableColumnCount;

/** @type {number} */
chrome.automation.AutomationNode.prototype.tableRowCount;

/** @type {number} */
chrome.automation.AutomationNode.prototype.hierarchicalLevel;

/**
 * @extends {chrome.automation.AutomationNode}
 * @constructor
 */
chrome.automation.AutomationRootNode = function() {};

/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationRootNode.prototype.anchorObject;

/**
 * @type {number}
 */
chrome.automation.AutomationRootNode.prototype.anchorOffset;

/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationRootNode.prototype.focusObject;

/**
 * @type {number}
 */
chrome.automation.AutomationRootNode.prototype.focusOffset;

/** @type {function() : !Object} */
chrome.app.getDetails;
