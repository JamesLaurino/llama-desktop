import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    property bool mono: false

    implicitHeight: Theme.controlHeight
    color: Theme.text
    placeholderTextColor: Theme.textDim
    selectionColor: Theme.accent
    selectedTextColor: "#0E1117"
    font.family: mono ? Theme.fontMono : Theme.fontUi
    font.pixelSize: Theme.fontSize
    leftPadding: Theme.s2
    rightPadding: Theme.s2
    verticalAlignment: Text.AlignVCenter

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.enabled ? Theme.bg : Theme.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.accent
                                          : (control.hovered ? Qt.lighter(Theme.border, 1.4)
                                                             : Theme.border)

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.hoverMs
                easing.type: Easing.OutCubic
            }
        }
    }
}
