pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Barre toujours visible (§5.4). La commande est recalculée de façon synchrone à
// chaque frappe : CommandBuilder::build() est une fonction pure, il n'y a rien à
// différer.
Rectangle {
    id: root

    color: Theme.surface
    implicitHeight: layout.implicitHeight + 2 * Theme.s4

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    FontMetrics {
        id: metrics
        font.family: Theme.fontMono
        font.pixelSize: Theme.fontSize
    }

    // Retour « Copié ✓ » pendant 1,5 s (§5.4).
    Timer {
        id: copiedFeedback
        interval: 1500
    }

    ImportDialog {
        id: importDialog
    }

    ConfirmDialog {
        id: relaunchConfirm
        title: qsTr("Un processus tourne déjà")
        message: qsTr("Un seul processus llama.cpp peut tourner à la fois. Arrêter celui en cours et lancer ce profil ?")
        confirmText: qsTr("Arrêter et lancer")
        onConfirmed: App.stopThenLaunch()
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.s4
        spacing: Theme.s3

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(
                    Math.max(commandText.contentHeight, metrics.height) + 2 * Theme.s2,
                    4 * metrics.height + 2 * Theme.s2)
                radius: Theme.radiusControl
                color: Theme.bg
                border.width: 1
                border.color: Theme.border
                clip: true

                Flickable {
                    id: flick
                    anchors.fill: parent
                    anchors.margins: Theme.s2
                    contentWidth: width
                    contentHeight: commandText.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    TextEdit {
                        id: commandText
                        width: flick.width
                        readOnly: true
                        selectByMouse: true
                        text: App.commandLine.length > 0
                              ? App.commandLine
                              : qsTr("Sélectionne ou crée un profil pour voir la commande.")
                        color: App.commandLine.length > 0 ? Theme.text : Theme.textDim
                        selectionColor: Theme.accent
                        selectedTextColor: "#0E1117"
                        wrapMode: TextEdit.WrapAnywhere
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSize
                    }
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: Theme.s2

                FlatButton {
                    Layout.preferredWidth: 120
                    text: copiedFeedback.running ? qsTr("Copié ✓") : qsTr("Copier")
                    enabled: App.commandLine.length > 0
                    onClicked: {
                        App.copyCommand()
                        copiedFeedback.restart()
                    }
                }

                // Chemin inverse, au même endroit que « Copier » : la symétrie
                // des deux sens se lit dans la disposition.
                FlatButton {
                    Layout.preferredWidth: 120
                    text: qsTr("Importer")
                    onClicked: importDialog.openWith("")
                }

                RowLayout {
                    spacing: Theme.s2

                    FlatButton {
                        Layout.preferredWidth: 120
                        variant: "primary"
                        text: qsTr("Lancer")
                        enabled: App.canLaunch
                        // Un seul processus à la fois (§10) : relancer propose
                        // d'arrêter le précédent plutôt que de refuser.
                        onClicked: {
                            if (App.running)
                                relaunchConfirm.open()
                            else
                                App.launch()
                        }

                        ToolTip.visible: hovered && !App.canLaunch
                                         && App.validationError.length > 0
                        ToolTip.delay: Theme.tooltipDelay
                        ToolTip.text: App.validationError
                    }

                    FlatButton {
                        variant: "danger"
                        visible: App.running
                        // terminate() ne fait rien sur un programme console
                        // Windows : le second clic n'attend plus la fin du délai
                        // de grâce et tue.
                        text: App.stopping ? qsTr("Forcer") : qsTr("Arrêter")
                        onClicked: App.stopProcess()
                    }
                }

                FlatButton {
                    Layout.preferredWidth: 120
                    variant: "ghost"
                    text: qsTr("Journal")
                    visible: !App.logsVisible && App.logs.count > 0
                    onClicked: App.logsVisible = true
                }
            }
        }

        // Les bandeaux d'avertissement du §8 arrivent avec la validation
        // complète ; seul le motif bloquant est affiché ici.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            visible: App.validationError.length > 0

            Text {
                text: "!"
                color: Theme.warn
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSize
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: App.validationError
                color: Theme.textDim
                wrapMode: Text.WordWrap
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
