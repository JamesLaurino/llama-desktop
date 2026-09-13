pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

// Panneau de gauche : la liste des profils, triée par dernière utilisation, avec
// son menu contextuel (§5.2). La recherche et la duplication au clavier arrivent
// en phase 5.
Rectangle {
    id: root

    color: Theme.surface
    border.width: 0

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.s4
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Profils")
            color: Theme.text
            font.family: Theme.fontUi
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Font.DemiBold
        }

        FlatButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.s3
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 30
            text: "+"
            onClicked: {
                const id = App.createProfile()
                namePrompt.targetId = id
                namePrompt.openWith(App.profileName)
            }
        }
    }

    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.rightMargin: 1
        clip: true
        model: App.profiles
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: delegateRoot

            required property string profileId
            required property string name
            required property string modelFileName
            required property string modelPath
            required property string binary
            required property string lastUsed
            required property bool running

            readonly property bool current: App.currentProfileId === profileId

            width: list.width
            height: 56

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Theme.radiusControl
                color: delegateRoot.current ? Theme.selected
                                            : (itemArea.containsMouse ? Theme.hover : "transparent")

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.hoverMs
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: parent.height - 16
                radius: 2
                color: Theme.accent
                visible: delegateRoot.current
            }

            Column {
                anchors.left: parent.left
                anchors.leftMargin: Theme.s4
                anchors.right: runningDot.left
                anchors.rightMargin: Theme.s2
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    width: parent.width
                    text: delegateRoot.name
                    color: Theme.text
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: delegateRoot.modelFileName
                    color: Theme.textDim
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideMiddle
                }
            }

            // Point vert animé : la phase 4 le fera vivre, le rôle vaut faux
            // tant qu'aucun processus n'est piloté.
            Rectangle {
                id: runningDot
                anchors.right: parent.right
                anchors.rightMargin: Theme.s4
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
                color: Theme.ok
                visible: delegateRoot.running

                SequentialAnimation on opacity {
                    running: delegateRoot.running
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 700; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutSine }
                }
            }

            MouseArea {
                id: itemArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    App.selectProfile(delegateRoot.profileId)
                    if (mouse.button === Qt.RightButton)
                        contextMenu.popup()
                }
            }

            ThemedMenu {
                id: contextMenu

                MenuItem {
                    text: qsTr("Renommer")
                    onTriggered: {
                        namePrompt.targetId = delegateRoot.profileId
                        namePrompt.openWith(delegateRoot.name)
                    }
                }

                MenuItem {
                    text: qsTr("Dupliquer")
                    onTriggered: App.duplicateProfile(delegateRoot.profileId)
                }

                MenuItem {
                    text: qsTr("Révéler le fichier modèle")
                    enabled: delegateRoot.modelPath.length > 0
                    onTriggered: App.revealModelFile(delegateRoot.profileId)
                }

                MenuSeparator {}

                MenuItem {
                    text: qsTr("Supprimer")
                    onTriggered: {
                        deleteDialog.targetId = delegateRoot.profileId
                        deleteDialog.message = qsTr("Supprimer définitivement « %1 » ? Cette action est irréversible.")
                                               .arg(delegateRoot.name)
                        deleteDialog.open()
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: list
        width: list.width - 2 * Theme.s5
        visible: list.count === 0
        text: qsTr("Aucun profil. Le bouton « + » en crée un.")
        color: Theme.textDim
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font.family: Theme.fontUi
        font.pixelSize: Theme.fontSize
    }

    NamePrompt {
        id: namePrompt
        property string targetId: ""
        title: qsTr("Nom du profil")
        onSubmitted: value => App.renameProfile(namePrompt.targetId, value)
    }

    ConfirmDialog {
        id: deleteDialog
        property string targetId: ""
        title: qsTr("Supprimer le profil")
        onConfirmed: App.removeProfile(deleteDialog.targetId)
    }
}
