pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// Fenêtre des réglages (§5.6). Les indicateurs ✓ / ✗ sont réévalués à chaque
// frappe puisqu'ils dépendent du texte du champ.
Window {
    id: root

    width: 660
    height: 420
    minimumWidth: 560
    minimumHeight: 380
    title: qsTr("Réglages — LlamaBuilder")
    color: Theme.bg

    function reload() {
        serverField.text = App.llamaServerPath
        cliField.text = App.llamaCliPath
        modelsField.text = App.defaultModelsDir
        intervalField.text = String(App.monitorIntervalMs)
    }

    onVisibleChanged: {
        if (visible)
            reload()
    }

    component PathRow: RowLayout {
        id: pathRow

        property alias text: field.text
        property string placeholder: ""
        property bool directory: false
        property bool valid: App.pathExists(field.text)

        spacing: Theme.s2

        ThemedTextField {
            id: field
            Layout.fillWidth: true
            mono: true
            placeholderText: pathRow.placeholder
        }

        Text {
            Layout.preferredWidth: 16
            text: pathRow.text.length === 0 ? "" : (pathRow.valid ? "✓" : "✗")
            color: pathRow.valid ? Theme.ok : Theme.danger
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeTitle
            horizontalAlignment: Text.AlignHCenter
        }

        FlatButton {
            text: qsTr("Parcourir")
            onClicked: pathRow.directory ? folderDialog.open() : fileDialog.open()
        }

        FileDialog {
            id: fileDialog
            title: pathRow.placeholder
            nameFilters: [qsTr("Exécutables (*.exe)"), qsTr("Tous les fichiers (*)")]
            onAccepted: pathRow.text = App.localFile(selectedFile)
        }

        FolderDialog {
            id: folderDialog
            title: pathRow.placeholder
            onAccepted: pathRow.text = App.localFile(selectedFolder)
        }
    }

    Flickable {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: form.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: form
            width: scroll.width
            spacing: Theme.s4

            Item {
                Layout.fillWidth: true
                implicitHeight: Theme.s2
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                columns: 2
                columnSpacing: Theme.s4
                rowSpacing: Theme.s3

                Text {
                    text: qsTr("llama-server.exe")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                }

                PathRow {
                    id: serverField
                    Layout.fillWidth: true
                    placeholder: qsTr("Chemin de llama-server.exe")
                }

                Text {
                    text: qsTr("llama-cli.exe")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                }

                PathRow {
                    id: cliField
                    Layout.fillWidth: true
                    placeholder: qsTr("Chemin de llama-cli.exe")
                }

                Text {
                    text: qsTr("Dossier des modèles")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                }

                PathRow {
                    id: modelsField
                    Layout.fillWidth: true
                    directory: true
                    placeholder: qsTr("Dossier proposé par défaut au choix d'un modèle")
                }

                Text {
                    text: qsTr("Rafraîchissement des jauges")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2

                    ThemedTextField {
                        id: intervalField
                        Layout.preferredWidth: 110
                        validator: IntValidator {
                            bottom: 250
                            top: 10000
                        }
                    }

                    Text {
                        text: qsTr("ms (250 à 10 000) — utilisé par la phase 3")
                        color: Theme.textDim
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                Text {
                    text: qsTr("Thème")
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2

                    Segmented {
                        id: themeSelector
                        value: "dark"
                        options: [{ label: qsTr("Sombre"), value: "dark" }]
                    }

                    Text {
                        text: qsTr("le cahier des charges ne définit qu'une palette sombre")
                        color: Theme.textDim
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                Layout.preferredHeight: diagnostics.implicitHeight + 2 * Theme.s3
                radius: Theme.radiusCard
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Column {
                    id: diagnostics
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.s3
                    anchors.rightMargin: Theme.s3
                    spacing: Theme.s1

                    Text {
                        width: parent.width
                        text: qsTr("Données : ") + App.dataDir
                        color: Theme.textDim
                        elide: Text.ElideMiddle
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Text {
                        width: parent.width
                        text: qsTr("params.json : ") + App.nativePath(App.paramsSource)
                        color: Theme.textDim
                        elide: Text.ElideMiddle
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Text {
                        width: parent.width
                        visible: App.targetBuild.length > 0
                        text: qsTr("build de référence : ") + App.targetBuild
                        color: Theme.textDim
                        elide: Text.ElideRight
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s5
                Layout.rightMargin: Theme.s5
                Layout.bottomMargin: Theme.s4
                spacing: Theme.s2

                Item {
                    Layout.fillWidth: true
                }

                FlatButton {
                    text: qsTr("Fermer")
                    onClicked: root.close()
                }

                FlatButton {
                    variant: "primary"
                    text: qsTr("Appliquer")
                    onClicked: {
                        App.applySettings(serverField.text, cliField.text, modelsField.text,
                                          parseInt(intervalField.text, 10) || 1000,
                                          themeSelector.value)
                        root.close()
                    }
                }
            }
        }
    }
}
