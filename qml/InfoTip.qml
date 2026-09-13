import QtQuick
import QtQuick.Controls.Basic

// Icône (?) et son info-bulle : le flag en monospace, l'explication en français,
// le défaut. 300 ms d'attente, 360 px au maximum, aucune troncature (§5.3).
Item {
    id: root

    property string flag: ""
    property string explanation: ""
    property string defaultValue: ""

    implicitWidth: 15
    implicitHeight: 15

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: hover.hovered ? Theme.accent : "transparent"
        border.width: 1
        border.color: hover.hovered ? Theme.accent : Theme.textDim

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverMs
                easing.type: Easing.OutCubic
            }
        }

        Text {
            anchors.centerIn: parent
            text: "?"
            color: hover.hovered ? "#0E1117" : Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.WhatsThisCursor
    }

    ToolTip {
        visible: hover.hovered
        delay: Theme.tooltipDelay
        padding: Theme.s3
        x: root.width + Theme.s2
        y: -Theme.s2

        contentItem: Column {
            spacing: Theme.s2

            Text {
                text: root.flag
                visible: root.flag.length > 0
                color: Theme.accent
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSize
            }

            Text {
                // 320 px de contenu + 2 × 12 px de marge : la bulle reste sous
                // les 360 px imposés, et le texte est replié, jamais coupé.
                width: Math.min(implicitWidth, Theme.tooltipMaxWidth - 2 * Theme.s3 - 16)
                text: root.explanation
                visible: root.explanation.length > 0
                color: Theme.text
                wrapMode: Text.WordWrap
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSize
                lineHeight: 1.25
            }

            Text {
                text: root.defaultValue.length > 0
                      ? qsTr("défaut : ") + root.defaultValue
                      : qsTr("aucun défaut déclaré")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: Theme.surfaceAlt
            border.width: 1
            border.color: Theme.border
        }
    }
}
