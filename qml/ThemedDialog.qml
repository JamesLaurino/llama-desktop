import QtQuick
import QtQuick.Controls.Basic

Dialog {
    id: control

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    padding: Theme.s4
    closePolicy: Popup.CloseOnEscape

    header: Text {
        text: control.title
        visible: control.title.length > 0
        color: Theme.text
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Font.DemiBold
        leftPadding: Theme.s4
        topPadding: Theme.s4
        bottomPadding: Theme.s2
    }

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.55)
    }
}
