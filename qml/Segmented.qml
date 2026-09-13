pragma ComponentBehavior: Bound

import QtQuick

// Sélecteur à N positions. Sert au tristate (Défaut / Activé / Désactivé), pour
// lequel aucun contrôle standard ne convient, et au choix du binaire.
Rectangle {
    id: root

    // [{ label: "...", value: "..." }, ...]
    property var options: []
    property string value: ""
    signal picked(string value)

    implicitWidth: row.implicitWidth
    implicitHeight: Theme.controlHeight
    radius: Theme.radiusControl
    color: Theme.bg
    border.width: 1
    border.color: Theme.border
    clip: true

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Repeater {
            model: root.options

            delegate: Rectangle {
                id: segment

                required property var modelData
                required property int index

                readonly property bool current: root.value === segment.modelData.value

                width: Math.max(64, label.implicitWidth + 2 * Theme.s3)
                height: row.height
                color: segment.current ? Theme.selected
                                       : (area.containsMouse ? Theme.hover : "transparent")

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.hoverMs
                        easing.type: Easing.OutCubic
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    visible: segment.index > 0
                    color: Theme.border
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: segment.modelData.label
                    color: segment.current ? Theme.text : Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                    font.weight: segment.current ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    id: area
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.picked(segment.modelData.value)
                }
            }
        }
    }
}
