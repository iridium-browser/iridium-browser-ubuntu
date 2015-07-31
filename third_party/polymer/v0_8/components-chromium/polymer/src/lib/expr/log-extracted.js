

  Base._addFeature({
    log: function() {
      var args = Array.prototype.slice.call(arguments, 0);
      args[0] = '[%s]: ' + args[0];
      args.splice(1, 0, this.localName);
      console.log.apply(console, args);
    }
  });

