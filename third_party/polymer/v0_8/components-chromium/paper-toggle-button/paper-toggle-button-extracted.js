
    Polymer({
      is: 'paper-toggle-button',

      behaviors: [
        Polymer.PaperButtonBehavior
      ],

      // The custom properties shim is currently an opt-in feature.
      enableCustomStyleProperties: true,

      hostAttributes: {
        role: 'button',
        'aria-pressed': 'false',
        tabindex: 0
      },

      properties: {
        /**
         * Fired when the checked state changes due to user interaction.
         *
         * @event change
         */
        /**
         * Fired when the checked state changes.
         *
         * @event iron-change
         */
        /**
         * Gets or sets the state, `true` is checked and `false` is unchecked.
         *
         * @attribute checked
         * @type boolean
         * @default false
         */
        checked: {
          type: Boolean,
          value: false,
          reflectToAttribute: true,
          observer: '_checkedChanged'
        }
      },

      listeners: {
        // TODO(sjmiles): tracking feature disabled until we can control
        // track/tap interaction with confidence
        //xtrack: '_ontrack'
      },

      ready: function() {
        this.toggles = true;
      },

      // button-behavior hook
      _buttonStateChanged: function() {
        this.checked = this.active;
      },

      _checkedChanged: function(checked) {
        this.active = this.checked;
        this.fire('iron-change');
      },

      _ontrack: function(event) {
        var track = event.detail;
        if (track.state === 'start' ) {
          //this._preventTap = true;
          this._trackStart(track);
        } else if (track.state === 'move' ) {
          this._trackMove(track);
        } else if (track.state === 'end' ) {
          this._trackEnd(track);
        }
      },

      _trackStart: function(track) {
        this._width = this.$.toggleBar.offsetWidth / 2;
        this._startx = track.x;
      },

      _trackMove: function(track) {
        var dx = track.x - this._startx;
        this._x = Math.min(this._width,
            Math.max(0, this.checked ? this._width + dx : dx));
        this.$.toggleButton.classList.add('dragging');
        this.translate3d(this, this._x + 'px', 0, 0);
      },

      _trackEnd: function(track) {
        this.$.toggleButton.classList.remove('dragging');
        this.transform(this, '');
        this._userActivate(Math.abs(this._x) > this._width / 2);
      }

    });
  