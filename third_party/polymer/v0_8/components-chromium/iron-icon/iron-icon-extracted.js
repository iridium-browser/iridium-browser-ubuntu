

  Polymer({

    is: 'iron-icon',

    enableCustomStyleProperties: true,

    properties: {

      icon: {
        type: String,
        observer: '_iconChanged'
      },

      theme: {
        type: String,
        observer: '_updateIcon'
      },

      src: {
        type: String,
        observer: '_srcChanged'
      }

    },

    _DEFAULT_ICONSET: 'icons',

    _iconChanged: function(icon) {
      var parts = (icon || '').split(':');
      this._iconName = parts.pop();
      this._iconsetName = parts.pop() || this._DEFAULT_ICONSET;
      this._updateIcon();
    },

    _srcChanged: function(src) {
      this._updateIcon();
    },

    _usesIconset: function() {
      return this.icon || !this.src;
    },

    _updateIcon: function() {
      if (this._usesIconset()) {
        this._iconset =  this.$.meta.byKey(this._iconsetName);
        if (this._iconset) {
          this._iconset.applyIcon(this, this._iconName, this.theme);
        } else {
          console.warn('iron-icon: could not find iconset `'
            + this._iconsetName + '`, did you import the iconset?');
        }
      } else {
        if (!this._img) {
          this._img = document.createElement('img');
          this._img.style.width = '100%';
          this._img.style.height = '100%';
        }
        this._img.src = this.src;
        Polymer.dom(this.root).appendChild(this._img);
      }
    }

  });

