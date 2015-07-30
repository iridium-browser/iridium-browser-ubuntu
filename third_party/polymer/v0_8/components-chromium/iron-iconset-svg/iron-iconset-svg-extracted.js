

  Polymer({

    is: 'iron-iconset-svg',

    properties: {

      /**
       * The name of the iconset.
       *
       * @attribute name
       * @type string
       * @default ''
       */
      name: {
        type: String,
        observer: '_nameChanged'
      },

      /**
       * Array of fully-qualitifed icon names in the iconset.
       */
      iconNames: {
        type: Array,
        notify: true
      },

      /**
       * The size of an individual icon. Note that icons must be square.
       *
       * @attribute iconSize
       * @type number
       * @default 24
       */
      size: {
        type: Number,
        value: 24
      }

    },

    /**
     * Applies an icon to the given element.
     *
     * An svg icon is prepended to the element's shadowRoot if it exists,
     * otherwise to the element itself.
     *
     * @method applyIcon
     * @param {Element} element Element to which the icon is applied.
     * @param {String} icon Name of the icon to apply.
     * @return {Element} The svg element which renders the icon.
     */
    applyIcon: function(element, iconName) {
      // insert svg element into shadow root, if it exists
      element = element.root || element;
      // Remove old svg element
      this.removeIcon(element);
      // install new svg element
      var svg = this._cloneIcon(iconName);
      if (svg) {
        // TODO(sjmiles): I know, `with` is the devil ... except it isn't
        with (Polymer.dom(element)) {
          insertBefore(svg, childNodes[0]);
        }
        return element._svgIcon = svg;
      }
    },

    /**
     * Remove an icon from the given element by undoing the changes effected
     * by `applyIcon`.
     *
     * @param {Element} element The element from which the icon is removed.
     */
    removeIcon: function(element) {
      // Remove old svg element
      if (element._svgIcon) {
        Polymer.dom(element).removeChild(element._svgIcon);
        element._svgIcon = null;
      }
    },

    /**
     *
     * When name is changed, either register a new iconset with the included
     * icons, or if there are no children, set up a meta-iconset.
     *
     */
    _nameChanged: function() {
      new Polymer.IronMeta({type: 'iconset', key: this.name, value: this});
      // icons (descendents) must exist a-priori
      this._icons = this._createIconMap();
      this.iconNames = this._getIconNames();
    },

    /**
     * Array of all icon names in this iconset.
     *
     * @return {Array} Array of icon names.
     */
    _getIconNames: function() {
       return Object.keys(this._icons).map(function(n) {
         return this.name + ':' + n;
       }, this);
    },

    /**
     * Create a map of child SVG elements by id.
     *
     * @return {Object} Map of id's to SVG elements.
     */
    _createIconMap: function() {
      // Objects chained to Object.prototype (`{}`) have members. Specifically,
      // on FF there is a `watch` method that confuses the icon map, so we
      // need to use a null-based object here.
      var icons = Object.create(null);
      Polymer.dom(this).querySelectorAll('[id]')
        .forEach(function(icon) {
          icons[icon.id] = icon;
        });
      return icons;
    },

    /**
     * Produce installable clone of the SVG element matching `id` in this
     * iconset, or `undefined` if there is no matching element.
     *
     * @return {Object} Returns an installable clone of the SVG element
     * matching `id`.
     */
    _cloneIcon: function(id) {
      return this._prepareSvgClone(this._icons[id], this.size);
    },

    _prepareSvgClone: function(sourceSvg, size) {
      if (sourceSvg) {
        var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('viewBox', ['0', '0', size, size].join(' '));
        svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');
        // TODO(dfreedm): `pointer-events: none` works around https://crbug.com/370136
        // TODO(sjmiles): inline style may not be ideal, but avoids requiring a shadow-root
        svg.style.cssText = 'pointer-events: none; display: block; width: 100%; height: 100%;';
        svg.appendChild(sourceSvg.cloneNode(true)).removeAttribute('id');
        return svg;
      }
    }

  });
