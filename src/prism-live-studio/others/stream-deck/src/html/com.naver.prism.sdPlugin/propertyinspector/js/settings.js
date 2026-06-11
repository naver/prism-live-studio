/**
 * update current select source id in settings
 * @param {Required} sourceId 
 */
function updateCurrentSourceId(sourceId, sceneId) {
    currentSourceID = sourceId;
    var result = getSceneItemIdBySourceSceneId(sceneId, sourceId);
    saveSettings({ key: "select_source_id", value: sourceId });
    if (result) {
        saveSettings({ key: "select_sceneItem_id", value: result.sceneItemId ? result.sceneItemId : -1 });
        saveSettings({ key: "select_groupSource_id", value: result.groupSourceId ? result.groupSourceId : "" });
    }
}

/**
 * get current streaming and recording state
 */
function getStreamingAndRecordingState() {
    const param = {
        id: RPC_ID_getRecordingAndStreamingState,
        method: "getModel",
        params: {
            resource: "StreamingService"
        }
    };
    sendValueToPlugin(param, "data");
}

/**
 * Below are a bunch of helpers to make your DOM interactive
 * The will cover changes of the most common elements in your DOM
 * and send their value to the plugin on a change.
 * To accomplish this, the 'handleSdpiItemChange' method tries to find the 
 * nearest element 'id' and the corresponding value of the element(along with
 * some other information you might need) . It then puts everything in a 
 * 'sdpi_collection', where the 'id' will get the 'key' and the 'value' will get the 'value'.
 * 
 * In the plugin you just need to listen for 'sdpi_collection' in the sent JSON.payload
 * and save the values you need in your settings (or StreamDeck-settings for persistence).
 * 
 * In this template those key/value pairs are saved automatically persistently to StreamDeck.
 * Open the console in the remote debugger to inspect what's getting saved.
 * 
 */

function saveSettings(sdpi_collection) {

    if (typeof sdpi_collection !== 'object') return;

    if (sdpi_collection.hasOwnProperty('key') && sdpi_collection.key != '') {
        if (sdpi_collection.value !== undefined) {
            console.log(sdpi_collection.key, " => ", sdpi_collection.value);
            settings[sdpi_collection.key] = sdpi_collection.value;
            console.log('setSettings....', settings);
            $SD.api.setSettings($SD.uuid, settings);
        }
    }
}

function saveGlobalSettings(data) {
    if (typeof data !== 'object') retuen;
    if (data.hasOwnProperty('key') && data.key != '') {
        console.log(data.key, " => ", data.value);
        settings[data.key] = data.value;
        console.log('setSettings....', settings);
        $SD.api.setGlobalSettings($SD.uuid, settings);
    }
}