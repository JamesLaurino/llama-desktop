pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Panneau de logs du §5.5.
//
// Il pousse le formulaire vers le haut plutôt que de le recouvrir : un tiroir
// masquerait les jauges, précisément ce qu'on regarde pendant un chargement.
Rectangle {
    id: root

    property int openHeight: 300

    // La hauteur est animée par une propriété intermédiaire : un Behavior ne se
    // pose pas sur une propriété attachée comme Layout.preferredHeight.
    property real revealed: App.logsVisible ? root.openHeight : 0
    Behavior on revealed {
        NumberAnimation {
            duration: Theme.sectionMs
            easing.type: Easing.OutCubic
        }
    }

    Layout.preferredHeight: root.revealed
    visible: root.revealed > 0
    clip: true
    color: Theme.surface

    Timer {
        id: copiedFeedback
        interval: 1500
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s3
        anchors.leftMargin: Theme.s5
        anchors.rightMargin: Theme.s5
        spacing: Theme.s2

        // --- En-tête ---------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: {
                    if (App.runFailed)
                        return Theme.danger
                    if (App.running)
                        return App.stopping ? Theme.warn : Theme.ok
                    return Theme.textDim
                }
            }

            Text {
                Layout.fillWidth: true
                text: App.runStatus.length > 0 ? App.runStatus : qsTr("Aucune exécution")
                color: App.runFailed ? Theme.danger : Theme.text
                elide: Text.ElideRight
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Font.DemiBold
            }

            FlatButton {
                variant: "primary"
                text: qsTr("Ouvrir dans le navigateur")
                visible: App.serverUrl.length > 0
                onClicked: App.openServerInBrowser()
            }

            ThemedCheckBox {
                id: autoScroll
                text: qsTr("Défilement auto")
                checked: true
            }

            FlatButton {
                text: copiedFeedback.running ? qsTr("Copié ✓") : qsTr("Copier tout")
                enabled: App.logs.count > 0
                onClicked: {
                    App.copyLogs()
                    copiedFeedback.restart()
                }
            }

            FlatButton {
                variant: "ghost"
                text: qsTr("Masquer")
                onClicked: App.logsVisible = false
            }
        }

        // --- Flux ------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusControl
            color: Theme.bg
            border.width: 1
            border.color: App.runFailed ? Theme.danger : Theme.border
            clip: true

            ListView {
                id: logView
                anchors.fill: parent
                anchors.margins: Theme.s2
                model: App.logs
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}

                // Le défilement suit la fin tant que l'utilisateur ne l'a pas
                // décoché (§5.5).
                //
                // Les deux signaux sont nécessaires : les délégués ont une
                // hauteur variable (les lignes longues sont repliées), et à
                // l'arrivée d'une ligne la vue n'a pas encore mesuré celles qui
                // la précèdent. Se caler sur le seul comptage laisse les
                // dernières lignes sous le pli.
                onCountChanged: logView.followTail()
                onContentHeightChanged: logView.followTail()

                function followTail() {
                    if (autoScroll.checked)
                        logView.positionViewAtEnd()
                }

                delegate: Text {
                    id: logLine

                    required property string line
                    required property int severity

                    width: logView.width - Theme.s3
                    text: logLine.line
                    color: {
                        if (logLine.severity === 2)
                            return Theme.danger
                        if (logLine.severity === 1)
                            return Theme.warn
                        if (logLine.severity === 3)
                            return Theme.accent
                        return Theme.text
                    }
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Text {
                anchors.centerIn: parent
                visible: App.logs.count === 0
                text: qsTr("Le journal du processus s'affichera ici.")
                color: Theme.textDim
                font.family: Theme.fontUi
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }
}
