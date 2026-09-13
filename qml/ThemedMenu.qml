pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: control

    implicitWidth: 240
    padding: Theme.s1

    delegate: MenuItem {
        id: item

        implicitHeight: Theme.controlHeight
        implicitWidth: control.width

        contentItem: Text {
            leftPadding: Theme.s3
            rightPadding: Theme.s3
            text: item.text
            color: item.enabled ? Theme.text : Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSize
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusControl - 2
            color: item.highlighted ? Theme.selected : "transparent"
        }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.surfaceAlt
        border.width: 1
        border.color: Theme.border
    }
}
