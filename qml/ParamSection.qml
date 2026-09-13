pragma ComponentBehavior: Bound

import QtQuick

// Section repliable engendrée depuis params.json. Elle disparaît d'elle-même
// quand le filtre ne retient plus rien : c'est ainsi que « Serveur » s'efface en
// mode cli, sans condition écrite dans la vue (§5.3).
Rectangle {
    id: root

    required property string sectionId
    required property string sectionLabel
    property bool expanded: false
    // Faux quand aucun profil n'est sélectionné : rien à éditer.
    property bool active: true

    readonly property int setCount: App.sectionSetCounts[sectionId] !== undefined
                                    ? App.sectionSetCounts[sectionId] : 0

    visible: active && filter.count > 0
    implicitHeight: column.implicitHeight
    radius: Theme.radiusCard
    color: Theme.surface
    border.width: 1
    border.color: Theme.border
    clip: true

    ParamFilterModel {
        id: filter
        sourceModel: App.params
        section: root.sectionId
    }

    Column {
        id: column
        width: parent.width

        Item {
            id: header
            width: parent.width
            height: 44

            Rectangle {
                anchors.fill: parent
                color: headerArea.containsMouse ? Theme.hover : "transparent"

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.hoverMs
                        easing.type: Easing.OutCubic
                    }
                }
            }

            MouseArea {
                id: headerArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }

            Canvas {
                id: chevron
                x: Theme.s4
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 6
                rotation: root.expanded ? 0 : -90
                transformOrigin: Item.Center

                Behavior on rotation {
                    NumberAnimation {
                        duration: Theme.sectionMs
                        easing.type: Easing.OutCubic
                    }
                }

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = Theme.textDim
                    ctx.lineWidth = 1.4
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(width / 2, height)
                    ctx.lineTo(width, 0)
                    ctx.stroke()
                }
            }

            Text {
                anchors.left: chevron.right
                anchors.leftMargin: Theme.s3
                anchors.verticalCenter: parent.verticalCenter
                text: root.sectionLabel
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            Text {
                id: counter
                anchors.right: resetButton.left
                anchors.rightMargin: Theme.s3
                anchors.verticalCenter: parent.verticalCenter
                text: root.setCount > 0
                      ? root.setCount + " / " + filter.count
                      : filter.count + qsTr(" paramètres")
                color: root.setCount > 0 ? Theme.accent : Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }

            FlatButton {
                id: resetButton
                anchors.right: parent.right
                anchors.rightMargin: Theme.s4
                anchors.verticalCenter: parent.verticalCenter
                variant: "ghost"
                text: qsTr("Réinitialiser")
                enabled: root.setCount > 0
                onClicked: App.resetSection(root.sectionId)
            }
        }

        Item {
            id: bodyClip
            width: parent.width
            clip: true
            height: root.expanded ? content.implicitHeight + Theme.s4 : 0

            Behavior on height {
                NumberAnimation {
                    duration: Theme.sectionMs
                    easing.type: Easing.OutCubic
                }
            }

            Column {
                id: content
                x: Theme.s4
                width: parent.width - 2 * Theme.s4
                spacing: Theme.s1

                Repeater {
                    model: filter
                    delegate: ParamRow {}
                }
            }
        }
    }
}
