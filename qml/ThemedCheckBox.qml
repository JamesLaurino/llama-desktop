import QtQuick
import QtQuick.Controls.Basic

CheckBox {
    id: control

    implicitHeight: Theme.controlHeight
    font.family: Theme.fontUi
    font.pixelSize: Theme.fontSize
    spacing: Theme.s2

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: Theme.radiusControl - 2
        color: control.checked ? Theme.accent : Theme.bg
        border.width: 1
        border.color: control.checked ? Theme.accent
                                      : (control.hovered ? Qt.lighter(Theme.border, 1.4)
                                                         : Theme.border)

        Behavior on color {
            ColorAnimation {
                duration: Theme.hoverMs
                easing.type: Easing.OutCubic
            }
        }

        Canvas {
            anchors.centerIn: parent
            width: 11
            height: 9
            visible: control.checked
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = "#0E1117"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(0, height * 0.5)
                ctx.lineTo(width * 0.38, height)
                ctx.lineTo(width, 0)
                ctx.stroke()
            }
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
    }
}
