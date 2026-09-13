import QtQuick
import QtQuick.Layouts

// Carte des jauges du §5.1. Sans pilote NVIDIA, la ligne VRAM est remplacée par
// le motif du relevé et la jauge RAM continue de fonctionner (critère n°6).
Rectangle {
    id: root

    // Typée et non `var` : qmllint vérifie alors chaque propriété lue ci-dessous.
    readonly property MonitorController mon: App.monitor

    implicitHeight: content.implicitHeight + 2 * Theme.s3
    radius: Theme.radiusCard
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.s3
        spacing: Theme.s3

        Gauge {
            Layout.fillWidth: true
            visible: root.mon.gpuAvailable
            label: qsTr("VRAM")
            ratio: root.mon.vramRatio
            processRatio: root.mon.vramProcessRatio
            level: root.mon.vramLevel
            valueText: root.mon.vramText
            processText: root.mon.vramProcessText
        }

        Text {
            Layout.fillWidth: true
            visible: !root.mon.gpuAvailable
            text: root.mon.gpuError === "" ? qsTr("GPU NVIDIA non détecté.") : root.mon.gpuError
            color: Theme.textDim
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Gauge {
            Layout.fillWidth: true
            label: qsTr("RAM")
            ratio: root.mon.ramRatio
            processRatio: root.mon.ramProcessRatio
            level: root.mon.ramLevel
            valueText: root.mon.ramText
            processText: root.mon.ramProcessText
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.mon.gpuAvailable
            spacing: Theme.s2

            Text {
                Layout.fillWidth: true
                text: root.mon.gpuName
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideRight
            }

            Text {
                text: root.mon.gpuUtilisation < 0
                      ? "" : qsTr("GPU %1 %").arg(root.mon.gpuUtilisation)
                color: Theme.textDim
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
