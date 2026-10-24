import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A wavelength field small enough for a column of filters.
SpinBox {
    Material.roundedScale: theme.radius
    implicitWidth: 120
    implicitHeight: 40
    font.pixelSize: 14
    Material.containerStyle: Material.Outlined
}
