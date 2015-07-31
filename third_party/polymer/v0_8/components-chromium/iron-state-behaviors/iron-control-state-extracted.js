

  Polymer.IronControlState = {

    properties: {

      /**
       * If true, the element currently has focus.
       *
       * @attribute focused
       * @type boolean
       * @default false
       */
      focused: {
        type: Boolean,
        value: false,
        notify: true,
        readOnly: true,
        reflectToAttribute: true
      },

      /**
       * If true, the user cannot interact with this element.
       *
       * @attribute disabled
       * @type boolean
       * @default false
       */
      disabled: {
        type: Boolean,
        value: false,
        notify: true,
        observer: '_disabledChanged',
        reflectToAttribute: true
      },

      _oldTabIndex: {
        type: String
      }
    },

    observers: [
      '_changedControlState(focused, disabled)'
    ],

    listeners: {
      focus: '_focusHandler',
      blur: '_blurHandler'
    },

    ready: function() {
      // TODO(sjmiles): ensure read-only property is valued so the compound
      // observer will fire
      if (this.focused === undefined) {
        this._setFocused(false);
      }
    },

    _focusHandler: function() {
      this._setFocused(true);
    },

    _blurHandler: function() {
      this._setFocused(false);
    },

    _disabledChanged: function(disabled, old) {
      this.setAttribute('aria-disabled', disabled ? 'true' : 'false');
      this.style.pointerEvents = disabled ? 'none' : '';
      if (disabled) {
        this._oldTabIndex = this.tabIndex;
        this.focused = false;
        this.tabIndex = -1;
      } else if (this._oldTabIndex !== undefined) {
        this.tabIndex = this._oldTabIndex;
      }
    },

    _changedControlState: function() {
      // _controlStateChanged is abstract, follow-on behaviors may implement it
      if (this._controlStateChanged) {
        this._controlStateChanged();
      }
    }

  };

