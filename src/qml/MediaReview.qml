// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2023 Droidian Project
//
// Authors:
// Bardia Moshiri <fakeshell@bardia.tech>
// Erik Inkinen <erik.inkinen@gmail.com>
// Alexander Rutz <alex@familyrutz.com>
// Joaquin Philco <joaquinphilco@gmail.com>

import QtQuick 2.15
import QtMultimedia 5.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import Qt.labs.folderlistmodel 2.15
import Qt.labs.platform 1.1

Rectangle {
    id: viewRect
    property int index: -1
    property var lastImg: index == -1 ? "" : imgModel.get(viewRect.index, "fileUrl")
    property string currentFileUrl: viewRect.index === -1 || imgModel.get(viewRect.index, "fileUrl") === undefined ? "" : imgModel.get(viewRect.index, "fileUrl").toString()
    property var folder: cslate.state == "VideoCapture" ?
                         StandardPaths.writableLocation(StandardPaths.MoviesLocation) + "/droidian-camera" :
                         StandardPaths.writableLocation(StandardPaths.PicturesLocation) + "/droidian-camera"
    property var deletePopUp: "closed"
    signal closed
    color: "black"
    visible: false

    Connections {
        target: thumbnailGenerator

        function onThumbnailGenerated(image) {
            viewRect.lastImg = thumbnailGenerator.toQmlImage(image);
        }
    }

    FolderListModel {
        id: imgModel
        folder: viewRect.folder
        showDirs: false
        nameFilters: cslate.state == "VideoCapture" ? ["*.mkv"] : ["*.jpg"]

        onStatusChanged: {
            if (imgModel.status == FolderListModel.Ready) {
                viewRect.index = imgModel.count - 1
                if (cslate.state == "VideoCapture" && viewRect.currentFileUrl.endsWith(".mkv")) {
                    thumbnailGenerator.setVideoSource(viewRect.currentFileUrl)
                } else {
                    viewRect.lastImg = viewRect.currentFileUrl
                }
            }
        }
    }

    Loader {
        id: mediaLoader
        anchors.fill: parent
        sourceComponent: viewRect.index == -1 ? null :
                         imgModel.get(viewRect.index, "fileUrl").toString().endsWith(".mkv") ? videoOutputComponent : imageComponent
    }

    Component {
        id: imageComponent
        Image {
            anchors.fill: parent
            autoTransform: true
            transformOrigin: Item.Center
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: (viewRect.currentFileUrl && !viewRect.currentFileUrl.endsWith(".mkv")) ? viewRect.currentFileUrl : ""
        }
    }

    Component {
        id: videoOutputComponent
        Item {
            anchors.fill: parent
            property bool firstFramePlayed: false

            MediaPlayer {
                id: mediaPlayer
                autoPlay: true
                muted: true
                source: viewRect.currentFileUrl

                onSourceChanged: {
                    firstFramePlayed = false;
                    muted = true;
                    play();
                }

                onPositionChanged: {
                    if (position > 0 && !firstFramePlayed) {
                        pause();
                        firstFramePlayed = true;
                    }
                }
            }

            VideoOutput {
                anchors.fill: parent
                source: mediaPlayer
                visible: viewRect.currentFileUrl && viewRect.currentFileUrl.endsWith(".mkv")
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                        mediaPlayer.pause();
                    } else {
                        if (firstFramePlayed) {
                            mediaPlayer.muted = false;
                        }

                        mediaPlayer.play();
                    }
                }
            }
        }
    }

    Button {
        id: btnPrev
        implicitWidth: 60
        implicitHeight: 60
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        icon.name: "go-previous-symbolic"
        icon.width: Math.round(btnPrev.width * 0.5)
        icon.height: Math.round(btnPrev.height * 0.5)
        icon.color: "white"
        Layout.alignment : Qt.AlignHCenter

        visible: viewRect.index > 0
        enabled: deletePopUp === "closed"

        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }

        onClicked: {
            if ((viewRect.index - 1) >= 0 ) {
                viewRect.index = viewRect.index - 1
            }
        }
    }

    Button {
        id: btnNext
        implicitWidth: 60
        implicitHeight: 60
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        icon.name: "go-next-symbolic"
        icon.width: Math.round(btnNext.width * 0.5)
        icon.height: Math.round(btnNext.height * 0.5)
        icon.color: "white"
        Layout.alignment : Qt.AlignHCenter

        visible: viewRect.index < (imgModel.count - 1)
        enabled: deletePopUp === "closed"

        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }

        onClicked: {
            if ((viewRect.index + 1) <= (imgModel.count - 1)) {
                viewRect.index = viewRect.index + 1
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 70
        color: "#AA000000"
        visible: viewRect.index >= 0
        Text {
            text: (viewRect.index + 1) + " / " + imgModel.count

            anchors.fill: parent
            anchors.margins: 5
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: "white"
            font.bold: true
            style: Text.Raised
            styleColor: "black"
            font.pixelSize: 16
        }
    }

    RowLayout {
        width: parent.width
        height: 70
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        Button {
            id: btnClose
            implicitWidth: 70
            implicitHeight: 70
            icon.name: "camera-video-symbolic"
            icon.width: Math.round(btnClose.width * 0.8)
            icon.height: Math.round(btnClose.height * 0.8)
            icon.color: "white"

            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }

            onClicked: {
                viewRect.visible = false
                viewRect.index = imgModel.count - 1
                viewRect.closed();
            }
        }
    }

    Button {
        id: btnDelete
        implicitWidth: 70
        implicitHeight: 70
        anchors.bottom: parent.bottom
        anchors.right: parent.right 
        icon.name: "edit-delete-symbolic"
        icon.width: Math.round(btnDelete.width * 0.5)
        icon.height: Math.round(btnDelete.height * 0.5)
        icon.color: "white"
        visible: viewRect.index >= 0
        Layout.alignment: Qt.AlignHCenter

        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }

        onClicked: {
            deletePopUp = "opened"
            confirmationPopup.open()
        }
    }

    Popup {
        id: confirmationPopup
        width: 200
        height: 80
        background: Rectangle {
            border.color: "#444"
            color: "lightgrey"
            radius: 10
        }
        closePolicy: Popup.NoAutoClose
        x: (parent.width - width) / 2
        y: (parent.height - height)

        Column {
            anchors.centerIn: parent
            spacing: 10

            Text {
                text: viewRect.currentFileUrl.endsWith(".mkv") ? "  Delete Video": "  Delete Photo?"
                horizontalAlignment: parent.AlignHCenter
            }

            Row {
                spacing: 20
                Button {
                    text: "Yes"
                    width: 60
                    onClicked: {
                        if(fileManager.deleteImage(viewRect.currentFileUrl)){
                            viewRect.index = viewRect.index - 1
                        }
                        deletePopUp = "closed"
                        confirmationPopup.close()
                    }
                }
                Button {
                    text: "No"
                    width: 60
                    onClicked: {
                        deletePopUp = "closed"
                        confirmationPopup.close()
                    }
                }
            }
        }
    }

    Rectangle {
        id: pictureMetaData
        anchors.top: parent.top
        width: parent.width
        height: 60
        color: "#AA000000"
        visible: viewRect.index >= 0

        property string dateTimePart: viewRect.currentFileUrl.replace(/.*image(\d{8}_\d{4}).*\.jpg$/, "$1")

        Text {
            id: date
            text: parseInt(pictureMetaData.dateTimePart.substr(0, 4)) +"/"+ (parseInt(pictureMetaData.dateTimePart.substr(4, 2)) - 1) 
                                       +"/"+ parseInt(pictureMetaData.dateTimePart.substr(6, 2))

            anchors.fill: parent
            anchors.margins: 5
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: "white"
            font.bold: true
            style: Text.Raised 
            styleColor: "black"
            font.pixelSize: 16
        }
    }
}