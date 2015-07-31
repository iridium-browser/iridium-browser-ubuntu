

  /**
   * Creates a pseudo-custom-element that maps property values to bindings
   * in DOM.
   * 
   * `stamp` method creates an instance of the pseudo-element. The instance
   * references a document-fragment containing the stamped and bound dom
   * via it's `root` property. 
   *  
   */
  Polymer({

    is: 'x-template',
    extends: 'template',

    behaviors: [
      Polymer.Templatizer
    ],

    ready: function() {
      this.templatize(this);
    }

  });

