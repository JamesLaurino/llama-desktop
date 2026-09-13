pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

// Disposition du §5.1 : profils à gauche, formulaire au centre, barre de commande
// toujours visible en bas. La restauration de la géométrie arrive en phase 5.
ApplicationWindow {
    id: window

    width: 1360
    height: 880
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    color: Theme.bg
    title: App.hasProfile ? "LlamaBuilder — " + App.profileName : "LlamaBuilder"

    // Le §9 impose d'arrêter le sondage quand la fenêtre est minimisée. Le fil
    // du moniteur reste vivant et NVML initialisé : la reprise est immédiate.
    Binding {
        target: App.monitor
        property: "active"
        value: window.visibility !== Window.Minimized && window.visibility !== Window.Hidden
    }

    SettingsWindow {
        id: settingsWindow
    }

    NamePrompt {
        id: renamePrompt
        title: qsTr("Renommer le profil")
        onSubmitted: value => App.renameProfile(App.currentProfileId, value)
    }

    FileDialog {
        id: modelDialog
        title: qsTr("Choisir un modèle GGUF")
        currentFolder: App.fileUrl(App.defaultModelsDir)
        nameFilters: [qsTr("Modèles GGUF (*.gguf)"), qsTr("Tous les fichiers (*)")]
        onAccepted: App.modelPath = App.localFile(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- Bandeau supérieur ------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.s5
                anchors.verticalCenter: parent.verticalCenter
                text: "LlamaBuilder"
                color: Theme.text
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }

            FlatButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.s5
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("⚙ Réglages")
                onClicked: settingsWindow.show()
            }
        }

        // --- Avertissement de démarrage --------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: warningText.implicitHeight + 2 * Theme.s3
            visible: App.startupWarning.length > 0
            color: Qt.rgba(0.976, 0.443, 0.443, 0.12)

            Text {
                id: warningText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.s5
                anchors.rightMargin: Theme.s5
                text: App.startupWarning
                color: Theme.danger
                wrapMode: Text.WordWrap
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        // --- Corps ------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ProfilePanel {
                Layout.preferredWidth: 300
                Layout.minimumWidth: 240
                Layout.fillHeight: true
            }

            // Flickable plutôt que ScrollView : la largeur du contenu est fixée à
            // celle de la vue, aucune négociation possible avec les largeurs
            // implicites des dispositions imbriquées.
            Flickable {
                id: formScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: formColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                ColumnLayout {
                    id: formColumn
                    width: formScroll.width
                    spacing: Theme.s4

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: Theme.s1
                    }

                    // Jauges VRAM / RAM du §5.1. Le sondage est piloté par la
                    // visibilité de la fenêtre, voir `monitorActive` plus haut.
                    MonitorCard {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.s5
                        Layout.rightMargin: Theme.s5
                    }

                    // --- Carte « Profil » ----------------------------------
                    // Le chemin du modèle a son propre champ dans le profil :
                    // il n'est délibérément pas déclaré dans params.json, pour
                    // n'avoir qu'une seule source de vérité.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.s5
                        Layout.rightMargin: Theme.s5
                        Layout.preferredHeight: profileCard.implicitHeight + 2 * Theme.s4
                        visible: App.hasProfile
                        radius: Theme.radiusCard
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        GridLayout {
                            id: profileCard
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.s4
                            columns: 2
                            columnSpacing: Theme.s3
                            rowSpacing: Theme.s3

                            Text {
                                Layout.preferredWidth: Theme.labelWidth
                                Layout.maximumWidth: Theme.labelWidth
                                text: qsTr("Nom du profil")
                                color: Theme.text
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSize
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.s2

                                Text {
                                    Layout.fillWidth: true
                                    text: App.profileName
                                    color: Theme.text
                                    elide: Text.ElideRight
                                    font.family: Theme.fontUi
                                    font.pixelSize: Theme.fontSize
                                    font.weight: Font.DemiBold
                                }

                                FlatButton {
                                    variant: "ghost"
                                    text: qsTr("Renommer")
                                    onClicked: renamePrompt.openWith(App.profileName)
                                }
                            }

                            Text {
                                Layout.preferredWidth: Theme.labelWidth
                                Layout.maximumWidth: Theme.labelWidth
                                text: qsTr("Modèle GGUF")
                                color: Theme.text
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSize
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.s2

                                ThemedTextField {
                                    Layout.fillWidth: true
                                    mono: true
                                    text: App.modelPath
                                    placeholderText: qsTr("Chemin du fichier .gguf")
                                    onTextEdited: App.modelPath = text
                                }

                                Text {
                                    Layout.preferredWidth: 16
                                    text: App.modelPath.length === 0
                                          ? "" : (App.pathExists(App.modelPath) ? "✓" : "✗")
                                    color: App.pathExists(App.modelPath) ? Theme.ok : Theme.danger
                                    font.family: Theme.fontUi
                                    font.pixelSize: Theme.fontSizeTitle
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                FlatButton {
                                    text: qsTr("Parcourir")
                                    onClicked: modelDialog.open()
                                }
                            }

                            Text {
                                Layout.preferredWidth: Theme.labelWidth
                                Layout.maximumWidth: Theme.labelWidth
                                text: qsTr("Binaire")
                                color: Theme.text
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSize
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.s3

                                Segmented {
                                    value: App.binary
                                    options: [
                                        { label: qsTr("llama-server"), value: "server" },
                                        { label: qsTr("llama-cli"), value: "cli" }
                                    ]
                                    onPicked: v => App.binary = v
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Changer de binaire masque les paramètres qu'il n'accepte pas.")
                                    color: Theme.textDim
                                    wrapMode: Text.WordWrap
                                    font.family: Theme.fontUi
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                            }

                            Text {
                                Layout.preferredWidth: Theme.labelWidth
                                Layout.maximumWidth: Theme.labelWidth
                                text: qsTr("Arguments libres")
                                color: Theme.text
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSize
                            }

                            ThemedTextField {
                                Layout.fillWidth: true
                                mono: true
                                text: App.extraArgs
                                placeholderText: qsTr("Ajoutés tels quels à la fin de la commande")
                                onTextEdited: App.extraArgs = text
                            }

                            Text {
                                Layout.preferredWidth: Theme.labelWidth
                                Layout.maximumWidth: Theme.labelWidth
                                text: qsTr("Notes")
                                color: Theme.text
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSize
                            }

                            ThemedTextField {
                                Layout.fillWidth: true
                                text: App.notes
                                placeholderText: qsTr("Pour mémoire, jamais transmis à llama.cpp")
                                onTextEdited: App.notes = text
                            }
                        }
                    }

                    // --- Sections engendrées depuis params.json -------------
                    Repeater {
                        model: App.sections

                        delegate: ParamSection {
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.leftMargin: Theme.s5
                            Layout.rightMargin: Theme.s5
                            active: App.hasProfile
                            sectionId: modelData.id
                            sectionLabel: modelData.label
                            expanded: modelData.expanded
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: Theme.s4
                    }
                }
            }
        }

        CommandBar {
            Layout.fillWidth: true
        }
    }
}
