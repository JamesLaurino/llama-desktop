import QtQuick

// Une barre du §9 : libellé à gauche, barre pleine largeur de 8 px, valeurs à
// droite. La part du processus llama se superpose à l'occupation système dans
// une teinte plus soutenue.
Item {
    id: root

    required property string label
    required property real ratio
    required property real processRatio
    required property int level
    required property string valueText
    required property string processText

    implicitHeight: labelText.implicitHeight + Theme.s1 + Theme.gaugeHeight

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.top: parent.top
        width: 52
        text: root.label
        color: Theme.textDim
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSizeSmall
    }

    Text {
        anchors.left: labelText.right
        anchors.leftMargin: Theme.s2
        anchors.baseline: labelText.baseline
        text: root.valueText
        color: Theme.text
        font.family: Theme.fontMono
        font.pixelSize: Theme.fontSizeSmall
    }

    Text {
        anchors.right: parent.right
        anchors.baseline: labelText.baseline
        text: root.processText
        color: Theme.textDim
        font.family: Theme.fontMono
        font.pixelSize: Theme.fontSizeSmall
        visible: root.processText !== ""
    }

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.gaugeHeight
        radius: height / 2
        color: Theme.bg
        border.width: 1
        border.color: Theme.border

        Rectangle {
            id: fill
            width: track.width * root.ratio
            height: track.height
            radius: track.radius
            color: Theme.gaugeColor(root.level)
            opacity: 0.45

            Behavior on width {
                NumberAnimation {
                    duration: Theme.gaugeMs
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: Theme.gaugeMs
                }
            }
        }

        // Dessinée par-dessus la part système, pas à côté : la lecture reste
        // « llama occupe cette fraction du total ».
        Rectangle {
            width: Math.min(fill.width, track.width * root.processRatio)
            height: track.height
            radius: track.radius
            color: Theme.gaugeColor(root.level)

            Behavior on width {
                NumberAnimation {
                    duration: Theme.gaugeMs
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: Theme.gaugeMs
                }
            }
        }
    }
}
