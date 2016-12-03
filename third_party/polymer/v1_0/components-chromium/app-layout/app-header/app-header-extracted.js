Polymer({
      is: 'app-header',

      behaviors: [
        Polymer.AppScrollEffectsBehavior,
        Polymer.IronResizableBehavior
      ],

      properties: {
        /**
         * If true, the header will automatically collapse when scrolling down.
         * That is, the `primary` element remains visible when the header is fully condensed
         * whereas the rest of the elements will collapse below `primary` element.
         *
         * By default, the `primary` element is the first toolbar in the light DOM:
         *
         *```html
         * <app-header condenses>
         *   <app-toolbar>This toolbar remains on top</app-toolbar>
         *   <app-toolbar></app-toolbar>
         *   <app-toolbar></app-toolbar>
         * </app-header>
         * ```
         *
         * Additionally, you can specify which toolbar or element remains visible in condensed mode
         * by adding the `primary` attribute to that element. For example: if we want the last
         * toolbar to remain visible, we can add the `primary` attribute to it.
         *
         *```html
         * <app-header condenses>
         *   <app-toolbar></app-toolbar>
         *   <app-toolbar></app-toolbar>
         *   <app-toolbar primary>This toolbar remains on top</app-toolbar>
         * </app-header>
         * ```
         *
         * Note the `primary` element must be a child of `app-header`.
         */
        condenses: {
          type: Boolean,
          value: false
        },

        /**
         * Mantains the header fixed at the top so it never moves away.
         */
        fixed: {
          type: Boolean,
          value: false
        },

        /**
         * Slides back the header when scrolling back up.
         */
        reveals: {
          type: Boolean,
          value: false
        },

        /**
         * Displays a shadow below the header.
         */
        shadow: {
          type: Boolean,
          reflectToAttribute: true,
          value: false
        }
      },

      observers: [
        'resetLayout(isAttached, condenses, fixed)'
      ],

      listeners: {
        'iron-resize': '_resizeHandler'
      },

      /**
       * A cached offsetHeight of the current element.
       *
       * @type {number}
       */
      _height: 0,

      /**
       * The distance in pixels the header will be translated to when scrolling.
       *
       * @type {number}
       */
      _dHeight: 0,

      /**
       * The offsetTop of `_primaryEl`
       *
       * @type {number}
       */
      _primaryElTop: 0,

      /**
       * The element that remains visible when the header condenses.
       *
       * @type {HTMLElement}
       */
      _primaryEl: null,

      /**
       * The header's top value used for the `transformY`
       *
       * @type {number}
       */
      _top: 0,

      /**
       * The current scroll progress.
       *
       * @type {number}
       */
      _progress: 0,

      _wasScrollingDown: false,
      _initScrollTop: 0,
      _initTimestamp: 0,
      _lastTimestamp: 0,
      _lastScrollTop: 0,

      /**
       * The distance the header is allowed to move away.
       *
       * @type {number}
       */
      get _maxHeaderTop() {
        return this.fixed ? this._dHeight : this._height + 5;
      },

      /**
       * Returns a reference to the element that remains visible when the header condenses.
       *
       * @return {HTMLElement}?
       */
      _getPrimaryEl: function() {
        /** @type {HTMLElement} */
        var primaryEl;
        var nodes = Polymer.dom(this.$.content).getDistributedNodes();

        for (var i = 0; i < nodes.length; i++) {
          if (nodes[i].nodeType === Node.ELEMENT_NODE) {
            var node = /** @type {HTMLElement} */ (nodes[i]);
            if (node.hasAttribute('primary')) {
              primaryEl = node;
              break;
            } else if (!primaryEl) {
              primaryEl = node;
            }
          }
        }
        return primaryEl;
      },

      /**
       * Resets the layout. If you changed the size of app-header via CSS
       * you can notify the changes by either firing the `iron-resize` event
       * or calling `resetLayout` directly.
       *
       * @method resetLayout
       */
      resetLayout: function() {
        this.fire('app-header-reset-layout');

        this.debounce('_resetLayout', function() {
          // noop if the header isn't visible
          if (this.offsetWidth === 0 && this.offsetHeight === 0) {
            return;
          }

          var scrollTop = this._clampedScrollTop;
          var firstSetup = this._height === 0 || scrollTop === 0;
          var currentDisabled = this.disabled;

          this._height = this.offsetHeight;
          this._primaryEl = this._getPrimaryEl();
          this.disabled = true;

          // prepare for measurement
          if  (!firstSetup) {
            this._updateScrollState(0, true);
          }

          if (this._mayMove()) {
            this._dHeight = this._primaryEl ? this._height - this._primaryEl.offsetHeight : 0;
          } else {
            this._dHeight = 0;
          }

          this._primaryElTop = this._primaryEl ? this._primaryEl.offsetTop : 0;
          this._setUpEffect();

          if (firstSetup) {
            this._updateScrollState(scrollTop, true);
          } else {
            this._updateScrollState(this._lastScrollTop, true);
            this._layoutIfDirty();
          }
          // restore no transition
          this.disabled = currentDisabled;
        });
      },

      /**
       * Updates the scroll state.
       *
       * @param {number} scrollTop
       * @param {boolean=} forceUpdate (default: false)
       */
      _updateScrollState: function(scrollTop, forceUpdate) {
        if (this._height === 0) {
          return;
        }

        var progress = 0;
        var top = 0;
        var lastTop = this._top;
        var lastScrollTop = this._lastScrollTop;
        var maxHeaderTop = this._maxHeaderTop;
        var dScrollTop = scrollTop - this._lastScrollTop;
        var absDScrollTop = Math.abs(dScrollTop);
        var isScrollingDown = scrollTop > this._lastScrollTop;
        var now = Date.now();

        if (this._mayMove()) {
          top = this._clamp(this.reveals ? lastTop + dScrollTop : scrollTop, 0, maxHeaderTop);
        }

        if (scrollTop >= this._dHeight) {
          top = this.condenses && !this.fixed ? Math.max(this._dHeight, top) : top;
          this.style.transitionDuration = '0ms';
        }

        if (this.reveals && !this.disabled && absDScrollTop < 100) {
          // set the initial scroll position
          if (now - this._initTimestamp > 300 || this._wasScrollingDown !== isScrollingDown) {
            this._initScrollTop = scrollTop;
            this._initTimestamp = now;
          }

          if (scrollTop >= maxHeaderTop) {
            // check if the header is allowed to snap
            if (Math.abs(this._initScrollTop - scrollTop) > 30 || absDScrollTop > 10) {
              if (isScrollingDown && scrollTop >= maxHeaderTop) {
                top = maxHeaderTop;
              } else if (!isScrollingDown && scrollTop >= this._dHeight) {
                top = this.condenses && !this.fixed ? this._dHeight : 0;
              }
              var scrollVelocity = dScrollTop / (now - this._lastTimestamp);
              this.style.transitionDuration = this._clamp((top - lastTop) / scrollVelocity, 0, 300) + 'ms';
            } else {
              top = this._top;
            }
          }
        }

        if (this._dHeight === 0) {
          progress = scrollTop > 0 ? 1 : 0;
        } else {
          progress = top / this._dHeight;
        }

        if (!forceUpdate) {
          this._lastScrollTop = scrollTop;
          this._top = top;
          this._wasScrollingDown = isScrollingDown;
          this._lastTimestamp = now;
        }

        if (forceUpdate || progress !== this._progress || lastTop !== top || scrollTop === 0) {
          this._progress = progress;
          this._runEffects(progress, top);
          this._transformHeader(top);
        }
      },

      /**
       * Returns true if the current header is allowed to move as the user scrolls.
       *
       * @return {boolean}
       */
      _mayMove: function() {
        return this.condenses || !this.fixed;
      },

      /**
       * Returns true if the current header will condense based on the size of the header
       * and the `consenses` property.
       *
       * @return {boolean}
       */
      willCondense: function() {
        return this._dHeight > 0 && this.condenses;
      },

      /**
       * Returns true if the current element is on the screen.
       * That is, visible in the current viewport.
       *
       * @method isOnScreen
       * @return {boolean}
       */
      isOnScreen: function() {
        return this._height !== 0 && this._top < this._height;
      },

      /**
       * Returns true if there's content below the current element.
       *
       * @method isContentBelow
       * @return {boolean}
       */
      isContentBelow: function() {
        if (this._top === 0) {
          return this._clampedScrollTop > 0;
        }
        return this._clampedScrollTop - this._maxHeaderTop >= 0;
      },

      /**
       * Transforms the header.
       *
       * @param {number} y
       */
      _transformHeader: function(y) {
        this.translate3d(0, (-y) + 'px', 0);
        if (this._primaryEl && this.condenses && y >= this._primaryElTop) {
          this.translate3d(0, (Math.min(y, this._dHeight) - this._primaryElTop) + 'px', 0,
              this._primaryEl);
        }
      },

      _resizeHandler: function() {
        this.resetLayout();
      },

      _clamp: function(v, min, max) {
        return Math.min(max, Math.max(min, v));
      },

      _ensureBgContainers: function() {
        if (!this._bgContainer) {
          this._bgContainer = document.createElement('div');
          this._bgContainer.id = 'background';

          this._bgRear = document.createElement('div');
          this._bgRear.id = 'backgroundRearLayer';
          this._bgContainer.appendChild(this._bgRear);

          this._bgFront = document.createElement('div');
          this._bgFront.id = 'backgroundFrontLayer';
          this._bgContainer.appendChild(this._bgFront);

          Polymer.dom(this.root).insertBefore(this._bgContainer, this.$.contentContainer);
        }
      },

      _getDOMRef: function(id) {
        switch (id) {
          case 'backgroundFrontLayer':
            this._ensureBgContainers();
            return this._bgFront;
          case 'backgroundRearLayer':
            this._ensureBgContainers();
            return this._bgRear;
          case 'background':
            this._ensureBgContainers();
            return this._bgContainer;
          case 'title':
            return Polymer.dom(this).querySelector('[title]');
          case 'condensedTitle':
            return Polymer.dom(this).querySelector('[condensed-title]');
        }
        return null;
      },

      /**
       * Returns an object containing the progress value of the scroll effects
       * and the top position of the header.
       *
       * @method getScrollState
       * @return {Object}
       */
      getScrollState: function() {
        return { progress: this._progress, top: this._top };
      }

      /**
       * Fires when the layout of `app-header` changed.
       *
       * @event app-header-reset-layout
       */
    });