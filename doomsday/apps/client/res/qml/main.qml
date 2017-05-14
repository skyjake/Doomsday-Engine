import QtQuick 2.7
import Doomsday 1.0

Item {
    width: 320
    height: 480

    ClientWindow {
        id: mainWin
        x: 0
        y: 0
        width: parent.width
        height: parent.height
        focus: true
        /*SequentialAnimation on t {
            NumberAnimation { to: 1; duration: 2500; easing.type: Easing.InQuad }
            NumberAnimation { to: 0; duration: 2500; easing.type: Easing.OutQuad }
            loops: Animation.Infinite
            running: true
        }*/
        onTextEntryRequest: {
            console.log("text entry requested");
            keyInput.focus = true;
        }
        onTextEntryDismiss: {
            console.log("text entry dismissed");
            keyInput.focus = false;
            mainWin.focus = true;
        }
        
        Connections {
            target: Qt.inputMethod
            onVisibleChanged: {
                if (Qt.inputMethod.visible) {
                    mainWin.height = parent.height - Qt.inputMethod.keyboardRectangle.height;
                    console.log("update dimensions:", mainWin.height);
                    mainWin.dimensionsChanged();
                }
                else {
                    console.log("inputMethod visibility:", Qt.inputMethod.visible);
                    mainWin.userFinishedTextEntry();
                    keyInput.focus = false;
                    mainWin.focus = true;
                    mainWin.height = parent.height;
                    mainWin.dimensionsChanged();
                }
            }
        }
    }
    
    /*Rectangle {
        color: Qt.rgba(1, 1, 1, 0.7)
        radius: 10
        border.width: 1
        border.color: "white"
        anchors.fill: label
        anchors.margins: -10
    }

    Text {
        id: label
        color: "black"
        wrapMode: Text.WordWrap
        text: mainWin.activeFocus? "I have active focus!" : "I do not have active focus"
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
    }*/
    
    MultiPointTouchArea {
        anchors.fill: parent
        onPressed: mainWin.onTouchPressed(touchPoints)
        onUpdated: mainWin.onTouchUpdated(touchPoints)
        onReleased: mainWin.onTouchReleased(touchPoints)
    }
    
    TextInput {
        id: keyInput
        visible: false
        onTextChanged: {
            mainWin.userEnteredText(text);
        }
//        onFocusChanged: {
//            if (!focus) {
//                mainWin.height = parent.height;
//                mainWin.dimensionsChanged();
//            }
//        }
    }
}
