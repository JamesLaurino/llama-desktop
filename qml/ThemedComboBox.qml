pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control

    implicitHeight: Theme.controlHeight
    font.family: Theme.fontUi
    font.pixelSize: Theme.fontSize

    contentItem: Text {
        leftPadding: Theme.s2
        rightPadding: Theme.s2
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        x: control.width - width - Theme.s2
        y: (control.height - height) / 2
        width: 10
        height: 6
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

    // Le modèle est toujours un tableau de chaînes : un seul rôle, modelData.
    delegate: ItemDelegate {
        id: item

        required property string modelData
        required property int index

        width: control.width
        height: Theme.controlHeight
        highlighted: control.highlightedIndex === item.index

        contentItem: Text {
            text: item.modelData
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: item.highlighted ? Theme.selected : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2 * Theme.s1, 280)
        padding: Theme.s1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
        }

        background: Rectangle {
            radius: Theme.radiusControl
            color: Theme.surfaceAlt
            border.width: 1
            border.color: Theme.border
        }
    }
}
