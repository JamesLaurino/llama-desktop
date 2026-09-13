pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Chemin inverse du §7 : une ligne de commande collée redevient un profil.
//
// L'analyse est relancée à chaque frappe — `CommandParser::parse()` est pure et
// ne touche à rien — et rien n'est écrit avant que l'utilisateur ne choisisse
// entre remplacer le profil courant et en créer un.
ThemedDialog {
    id: control

    title: qsTr("Importer une ligne de commande")
    width: 760

    // Dernier résultat d'analyse. Vide tant que rien n'a été collé.
    property var analysis: ({})
    readonly property bool usable: analysis.ok === true && analysis.empty === false

    function openWith(initial) {
        source.text = initial !== undefined ? initial : ""
        control.analyse()
        control.open()
        source.forceActiveFocus()
    }

    function analyse() {
        control.analysis = App.analyseCommand(source.text)
        // Nom proposé : le fichier du modèle, c'est ce qui identifie le profil
        // dans la tête de l'utilisateur.
        if (nameField.text.length === 0 && control.analysis.modelPath) {
            const base = App.fileName(control.analysis.modelPath)
            nameField.text = base.replace(/\.gguf$/i, "")
        }
    }

    onOpened: control.analyse()

    contentItem: ColumnLayout {
        spacing: Theme.s3

        Text {
            Layout.fillWidth: true
            text: qsTr("Colle la commande telle quelle. Les formes longues, les « --option=valeur » et les retours à la ligne sont acceptés.")
            color: Theme.textDim
            wrapMode: Text.WordWrap
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeSmall
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            radius: Theme.radiusControl
            color: Theme.bg
            border.width: 1
            border.color: source.activeFocus ? Theme.accent : Theme.border
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: Theme.s2

                TextArea {
                    id: source
                    placeholderText: qsTr("llama-server.exe -m D:/modeles/qwen3.gguf --ctx-size 16384 -ngl 99")
                    color: Theme.text
                    placeholderTextColor: Theme.textDim
                    selectionColor: Theme.accent
                    selectedTextColor: "#0E1117"
                    wrapMode: TextArea.WrapAnywhere
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSize
                    background: null
                    onTextChanged: control.analyse()
                }
            }
        }

        // --- Refus explicite ------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            // Un champ encore vide n'est pas une erreur : le bandeau rouge ne
            // doit apparaître que sur une ligne réellement fautive.
            visible: control.analysis.ok === false && source.text.trim().length > 0
            radius: Theme.radiusControl
            color: Qt.rgba(0.973, 0.443, 0.443, 0.12)
            border.width: 1
            border.color: Theme.danger
            implicitHeight: errorText.implicitHeight + 2 * Theme.s2

            Text {
                id: errorText
                anchors.fill: parent
                anchors.margins: Theme.s2
                text: control.analysis.error !== undefined ? control.analysis.error : ""
                color: Theme.danger
                wrapMode: Text.WordWrap
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        // --- Aperçu ---------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            visible: control.analysis.ok === true
            radius: Theme.radiusControl
            color: Theme.bg
            border.width: 1
            border.color: Theme.border
            clip: true

            ScrollView {
                id: previewScroll
                anchors.fill: parent
                anchors.margins: Theme.s3
                contentWidth: availableWidth

                ColumnLayout {
                    width: previewScroll.availableWidth
                    spacing: Theme.s3

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: Theme.s3
                        rowSpacing: Theme.s1

                        Text {
                            text: qsTr("Binaire")
                            color: Theme.textDim
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Text {
                            Layout.fillWidth: true
                            text: control.analysis.binary
                                  ? control.analysis.binary
                                  : qsTr("non déduit — celui du profil est conservé")
                            color: control.analysis.binary ? Theme.text : Theme.textDim
                            elide: Text.ElideMiddle
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Text {
                            text: qsTr("Modèle")
                            color: Theme.textDim
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        Text {
                            Layout.fillWidth: true
                            text: control.analysis.modelPath
                                  ? App.nativePath(control.analysis.modelPath)
                                  : qsTr("aucun (-m absent)")
                            color: control.analysis.modelPath ? Theme.text : Theme.warn
                            elide: Text.ElideMiddle
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            const n = control.analysis.entries !== undefined
                                    ? control.analysis.entries.length : 0
                            return n === 0 ? qsTr("Aucun paramètre reconnu")
                                           : qsTr("%n paramètre(s) reconnu(s)", "", n)
                        }
                        color: Theme.text
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: control.analysis.entries !== undefined ? control.analysis.entries : []

                        RowLayout {
                            id: entryRow
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: Theme.s2

                            Text {
                                text: entryRow.modelData.flag
                                color: Theme.accent
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Text {
                                Layout.preferredWidth: 200
                                text: entryRow.modelData.label
                                color: entryRow.modelData.applies ? Theme.textDim : Theme.warn
                                elide: Text.ElideRight
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Text {
                                Layout.fillWidth: true
                                text: entryRow.modelData.value
                                color: Theme.text
                                elide: Text.ElideMiddle
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: control.analysis.extraArgs !== undefined
                                 && control.analysis.extraArgs.length > 0
                        text: qsTr("Arguments libres : %1").arg(control.analysis.extraArgs)
                        color: Theme.text
                        wrapMode: Text.WrapAnywhere
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    // Les avertissements sont la partie honnête de l'import : ce
                    // qui a été deviné ou déplacé doit se voir avant de valider.
                    Repeater {
                        model: control.analysis.notes !== undefined ? control.analysis.notes : []

                        RowLayout {
                            id: noteRow
                            required property string modelData

                            Layout.fillWidth: true
                            spacing: Theme.s2

                            Text {
                                text: "!"
                                color: Theme.warn
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: noteRow.modelData
                                color: Theme.warn
                                wrapMode: Text.WordWrap
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.border
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Commande que l'application produira")
                        color: Theme.textDim
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: control.analysis.preview !== undefined ? control.analysis.preview : ""
                        color: Theme.textDim
                        wrapMode: Text.WrapAnywhere
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2

            Text {
                text: qsTr("Nom du nouveau profil")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }

            ThemedTextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Commande importée")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2

            Item {
                Layout.fillWidth: true
            }

            FlatButton {
                text: qsTr("Annuler")
                onClicked: control.close()
            }

            FlatButton {
                text: qsTr("Remplacer le profil courant")
                enabled: control.usable && App.hasProfile
                onClicked: {
                    if (App.importCommandIntoCurrent(source.text))
                        control.close()
                }
            }

            FlatButton {
                variant: "primary"
                text: qsTr("Créer un profil")
                enabled: control.usable
                onClicked: {
                    if (App.importCommandAsNewProfile(source.text, nameField.text).length > 0)
                        control.close()
                }
            }
        }
    }
}
