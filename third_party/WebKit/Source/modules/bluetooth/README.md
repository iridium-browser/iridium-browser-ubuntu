Web Bluetooth
=============

`Source/modules/bluetooth` implements [Web Bluetooth][WB].

[WB]: https://webbluetoothcg.github.io/web-bluetooth/

Testing
--------------------------------------------------------------------------------

Bluetooth layout tests in `LayoutTests/bluetooth/` rely on
fake Bluetooth implementation classes constructed in
`content/shell/browser/layout_test/layout_test_bluetooth_adapter_provider`.
These tests span JavaScript binding to the `device/bluetooth` API layer.


Design Documents
--------------------------------------------------------------------------------

See: [Class Diagram of Web Bluetooth through Bluetooth Android][Class]

[Class]: https://sites.google.com/a/chromium.org/dev/developers/design-documents/bluetooth-design-docs/web-bluetooth-through-bluetooth-android-class-diagram

