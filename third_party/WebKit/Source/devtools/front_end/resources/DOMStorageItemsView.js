/*
 * Copyright (C) 2008 Nokia Inc.  All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
Resources.DOMStorageItemsView = class extends UI.SimpleView {
  constructor(domStorage) {
    super(Common.UIString('DOM Storage'));

    this.domStorage = domStorage;

    this.element.classList.add('storage-view', 'table');

    this.deleteButton = new UI.ToolbarButton(Common.UIString('Delete'), 'largeicon-delete');
    this.deleteButton.setVisible(false);
    this.deleteButton.addEventListener(UI.ToolbarButton.Events.Click, this._deleteButtonClicked, this);

    this.refreshButton = new UI.ToolbarButton(Common.UIString('Refresh'), 'largeicon-refresh');
    this.refreshButton.addEventListener(UI.ToolbarButton.Events.Click, this._refreshButtonClicked, this);

    this.domStorage.addEventListener(
        Resources.DOMStorage.Events.DOMStorageItemsCleared, this._domStorageItemsCleared, this);
    this.domStorage.addEventListener(
        Resources.DOMStorage.Events.DOMStorageItemRemoved, this._domStorageItemRemoved, this);
    this.domStorage.addEventListener(Resources.DOMStorage.Events.DOMStorageItemAdded, this._domStorageItemAdded, this);
    this.domStorage.addEventListener(
        Resources.DOMStorage.Events.DOMStorageItemUpdated, this._domStorageItemUpdated, this);
  }

  /**
   * @override
   * @return {!Array.<!UI.ToolbarItem>}
   */
  syncToolbarItems() {
    return [this.refreshButton, this.deleteButton];
  }

  /**
   * @override
   */
  wasShown() {
    this._update();
  }

  /**
   * @override
   */
  willHide() {
    this.deleteButton.setVisible(false);
  }

  /**
   * @param {!Common.Event} event
   */
  _domStorageItemsCleared(event) {
    if (!this.isShowing() || !this._dataGrid)
      return;

    this._dataGrid.rootNode().removeChildren();
    this._dataGrid.addCreationNode(false);
    this.deleteButton.setVisible(false);
  }

  /**
   * @param {!Common.Event} event
   */
  _domStorageItemRemoved(event) {
    if (!this.isShowing() || !this._dataGrid)
      return;

    var storageData = event.data;
    var rootNode = this._dataGrid.rootNode();
    var children = rootNode.children;

    for (var i = 0; i < children.length; ++i) {
      var childNode = children[i];
      if (childNode.data.key === storageData.key) {
        rootNode.removeChild(childNode);
        this.deleteButton.setVisible(children.length > 1);
        return;
      }
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _domStorageItemAdded(event) {
    if (!this.isShowing() || !this._dataGrid)
      return;

    var storageData = event.data;
    var rootNode = this._dataGrid.rootNode();
    var children = rootNode.children;

    this.deleteButton.setVisible(true);

    for (var i = 0; i < children.length; ++i) {
      if (children[i].data.key === storageData.key)
        return;
    }

    var childNode = new DataGrid.DataGridNode({key: storageData.key, value: storageData.value}, false);
    rootNode.insertChild(childNode, children.length - 1);
  }

  /**
   * @param {!Common.Event} event
   */
  _domStorageItemUpdated(event) {
    if (!this.isShowing() || !this._dataGrid)
      return;

    var storageData = event.data;
    var rootNode = this._dataGrid.rootNode();
    var children = rootNode.children;

    var keyFound = false;
    for (var i = 0; i < children.length; ++i) {
      var childNode = children[i];
      if (childNode.data.key === storageData.key) {
        if (keyFound) {
          rootNode.removeChild(childNode);
          return;
        }
        keyFound = true;
        if (childNode.data.value !== storageData.value) {
          childNode.data.value = storageData.value;
          childNode.refresh();
          childNode.select();
          childNode.reveal();
        }
        this.deleteButton.setVisible(true);
      }
    }
  }

  _update() {
    this.detachChildWidgets();
    this.domStorage.getItems(this._showDOMStorageItems.bind(this));
  }

  _showDOMStorageItems(error, items) {
    if (error)
      return;

    this._dataGrid = this._dataGridForDOMStorageItems(items);
    this._dataGrid.asWidget().show(this.element);
    this.deleteButton.setVisible(this._dataGrid.rootNode().children.length > 1);
  }

  _dataGridForDOMStorageItems(items) {
    var columns = /** @type {!Array<!DataGrid.DataGrid.ColumnDescriptor>} */ ([
      {id: 'key', title: Common.UIString('Key'), sortable: false, editable: true, weight: 50},
      {id: 'value', title: Common.UIString('Value'), sortable: false, editable: true, weight: 50}
    ]);

    var nodes = [];

    var keys = [];
    var length = items.length;
    for (var i = 0; i < items.length; i++) {
      var key = items[i][0];
      var value = items[i][1];
      var node = new DataGrid.DataGridNode({key: key, value: value}, false);
      node.selectable = true;
      nodes.push(node);
      keys.push(key);
    }

    var dataGrid = new DataGrid.DataGrid(columns, this._editingCallback.bind(this), this._deleteCallback.bind(this));
    dataGrid.setName('DOMStorageItemsView');
    length = nodes.length;
    for (var i = 0; i < length; ++i)
      dataGrid.rootNode().appendChild(nodes[i]);
    dataGrid.addCreationNode(false);
    if (length > 0)
      nodes[0].selected = true;
    return dataGrid;
  }

  /**
   * @param {!Common.Event} event
   */
  _deleteButtonClicked(event) {
    if (!this._dataGrid || !this._dataGrid.selectedNode)
      return;

    this._deleteCallback(this._dataGrid.selectedNode);
  }

  /**
   * @param {!Common.Event} event
   */
  _refreshButtonClicked(event) {
    this._update();
  }

  _editingCallback(editingNode, columnIdentifier, oldText, newText) {
    var domStorage = this.domStorage;
    if (columnIdentifier === 'key') {
      if (typeof oldText === 'string')
        domStorage.removeItem(oldText);
      domStorage.setItem(newText, editingNode.data.value || '');
      this._removeDupes(editingNode);
    } else {
      domStorage.setItem(editingNode.data.key || '', newText);
    }
  }

  /**
   * @param {!DataGrid.DataGridNode} masterNode
   */
  _removeDupes(masterNode) {
    var rootNode = this._dataGrid.rootNode();
    var children = rootNode.children;
    for (var i = children.length - 1; i >= 0; --i) {
      var childNode = children[i];
      if ((childNode.data.key === masterNode.data.key) && (masterNode !== childNode))
        rootNode.removeChild(childNode);
    }
  }

  _deleteCallback(node) {
    if (!node || node.isCreationNode)
      return;

    if (this.domStorage)
      this.domStorage.removeItem(node.data.key);
  }
};
