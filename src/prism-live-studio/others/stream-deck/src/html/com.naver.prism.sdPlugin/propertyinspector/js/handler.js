
function handleDidReceiveSettings(jsn) {

    if (jsn.hasOwnProperty("settings")) {
        //settings = jsn["settings"];
    }
}

// handle events from plugin
function handleEventsFromPlugin(jsn) {

    if (!jsn.payload) {
        return;
    }

    if (jsn.payload.hasOwnProperty("id")) {
        // get scene collection list
        const rpcId = jsn.payload["id"];
        const result = jsn.payload["data"];
        if (rpcId != null) {
            switch (rpcId) {
                case RPC_ID_fetchSceneCollectionsSchema: {
                    //saveSettings({ key: "select_collection_id", value: result["activeSceneCollection"] });
                    initializeCollectionList(result["collectionList"]);
                    break;
                }
                case RPC_ID_getScenes: {
                    console.log("RPC_ID_getScenes called");
                    initializeSceneList(result);
                    break;
                }
                case RPC_ID_getSources: {
                    console.log("RPC_ID_getSources called");
                    if (result != undefined) {
                        initializeAudioSourceList(result);
                    }
                    break;
                }
                case RPC_ID_getRecordingAndStreamingState: {
                    saveSettings({ key: "recordingStatus", value: result.recordingStatus });
                    saveSettings({ key: "streamingStatus", value: result.streamingStatus });
                } break;
                case RPC_ID_getSideWindowInfo: {
                    initializeWindowList(result);
                } break;
                case RPC_ID_updateSceneList: {
                    var collectionId = jsn.payload["param"];
                    if (collectionId == currentCollectionID) {
                        getSceneAndSource(currentCollectionID);
                    }
                } break;
                case RPC_ID_updateSourceList: {
                    var collectionId = jsn.payload["param"];
                    if (collectionId == currentCollectionID) {
                        getSourceList(currentCollectionID);
                    }
                } break;
                case RPC_ID_getPrismConnectionState: {
                    updatePropertyInspectorUi(result);
                } break;
                case RPC_ID_prismLoadingFinished: {
                    if (currentCollectionID != undefined && currentCollectionID != "") {
                        if (result != currentCollectionID) {
                            getSceneAndSource(currentCollectionID);
                        }
                    }
                } break;
            }
        } else {
            // events
            const data = result["data"];
            const resourceId = result["resourceId"];
            if (data != undefined) {
                switch (resourceId) {
                    case EVENT_SIDE_WINDOW_VISIBLE_CHANGED: {

                    } break;
                    default: break;
                }

            } else {
                switch (resourceId) {
                    case EVENT_SOURCE_UPDATED: {

                    } break;
                    default: break;
                }
            }
        }
    }
}

function initializeCollectionList(data) {
    const collection_el = document.querySelector('#select_collection_id');
    if (data == undefined) {
        collection_el.disabled = true;
        collection_el.innerHTML = select_none;
        initializeSceneList(null);
        initializeAudioSourceList(null);
        return;
    }

    const adjustCollectionSelect = (collectionArr, selectedID) => {
        return collectionArr.map(collect => `<option value="${encodeURIComponent(collect.id)}" ${collect.id == selectedID ? 'selected' : ''}> ${collect.name}</option >`).join(' ');
    };

    // save collection list data
    const jsn = {
        key: "collectionList",
        value: data
    };
    saveSettings(jsn);

    collection_el.innerHTML = adjustCollectionSelect(data, currentCollectionID);
    collection_el.onchange = () => {
        currentCollectionID = decodeURIComponent(collection_el.value);
        saveSettings({ key: "select_collection_id", value: currentCollectionID });
        saveSettings({ key: "audioSourceId", value: "" });
        getSceneAndSource(currentCollectionID);
        getSourceList(currentCollectionID);
    };

    // set current collection and update collection combobox
    if (collection_el.options.length > 0) {
        set_select_checked(collection_el, settings.select_collection_id);
        currentCollectionID = decodeURIComponent(collection_el.value);
        saveSettings({ key: "select_collection_id", value: currentCollectionID });
        getSceneAndSource(currentCollectionID);
        getSourceList(currentCollectionID);
        collection_el.disabled = false;
    } else {
        collection_el.disabled = true;
        collection_el.innerHTML = select_none;
        initializeSceneList(null);
        initializeAudioSourceList(null);
    }
}

function initializeSceneList(sceneList) {
    const scene_el = document.querySelector('#select_scene_id');
    const source_el = document.querySelector('#select_source_id');
    if (sceneList == undefined) {
        scene_el.disabled = true;
        scene_el.innerHTML = select_none;

        source_el.disabled = true;
        source_el.innerHTML = select_none;
        return;
    }
    const adjustSceneSelect = (sceneArr, selectedID) => {
        return sceneArr.map(collect => {
            return `<option value="${encodeURIComponent(collect.id)}" ${collect.id == selectedID ? 'selected' : ''}> ${collect.name}</option >`;
        }).join(' ');
    };

    // save scene list data
    const jsn = {
        key: "sceneList",
        value: sceneList
    };
    saveSettings(jsn);

    scene_el.innerHTML = adjustSceneSelect(sceneList, currentSceneID);
    scene_el.onchange = () => {
        currentSceneID = decodeURIComponent(scene_el.value);
        saveSettings({ key: "select_scene_id", value: currentSceneID });
        set_select_checked(scene_el, settings.select_scene_id);
        initializeSourceList(currentSceneID);
    };

    // set current scene and update scenes combobox
    if (scene_el.options.length > 0) {
        set_select_checked(scene_el, settings.select_scene_id);
        currentSceneID = decodeURIComponent(scene_el.value);
        saveSettings({ key: "select_scene_id", value: currentSceneID });
        scene_el.disabled = false;
        initializeSourceList(currentSceneID);
    } else {
        currentSceneID = '';
        saveSettings({ key: "select_scene_id", value: currentSceneID });
        scene_el.disabled = true;
        scene_el.innerHTML = select_none;

        source_el.disabled = true;
        source_el.innerHTML = select_none;
    }
}

function initializeAudioSourceList(sourceList) {
    const source_audio_el = document.querySelector('#select_audio_source_id');
    if (sourceList == undefined) {
        source_audio_el.disabled = true;
        source_audio_el.innerHTML = select_none;
        return;
    }
    const adjustSourceSelect = (arr, selectedID) => {
        return arr.map(collect => {
            if (collect.audio) {
                return `<option value="${encodeURIComponent(collect.id)}" ${collect.id == selectedID ? 'selected' : ''}> ${collect.name}</option >`;
            }
        }).join(' ');
    };

    // save source list data
    saveSettings({ key: "sourceList", value: sourceList });

    source_audio_el.innerHTML = adjustSourceSelect(sourceList, currentAudioSourceId);
    source_audio_el.onchange = () => {
        currentAudioSourceId = decodeURIComponent(source_audio_el.value);
        saveSettings({ key: "audioSourceId", value: currentAudioSourceId });
        set_select_checked(source_audio_el, currentAudioSourceId);
    };

    // set current source and update source list
    if (source_audio_el.options.length > 0) {
        set_select_checked(source_audio_el, settings.audioSourceId);
        currentAudioSourceId = decodeURIComponent(source_audio_el.value);
        saveSettings({ key: "audioSourceId", value: currentAudioSourceId });
        source_audio_el.disabled = false;
    } else {
        currentAudioSourceId = '';
        saveSettings({ key: "audioSourceId", value: currentAudioSourceId });
        source_audio_el.disabled = true;
        source_audio_el.innerHTML = select_none;
    }
}

function initializeWindowList(result) {
    const window_el = document.querySelector('#select_side_window_id');
    window_el.innerHTML = "";
    const adjustWindowList = (arr, selectedID) => {
        return arr.map(collect => `<option value="${encodeURIComponent(collect.id)}" ${collect.id == selectedID ? 'selected' : ''}> ${collect.window_name}</option >`).join(' ');
    };
    saveSettings({ key: "sideWindowInfo", value: result });
    window_el.onchange = () => {
        currentSelectWindowId = decodeURIComponent(window_el.value);
        saveSettings({ key: "selectSideWindowInfo", value: currentSelectWindowId });
    };
    window_el.innerHTML = adjustWindowList(result, "");

    /**
     * initial side window list
     */
    if (window_el.options.length > 0) {
        if (!settings.selectSideWindowInfo || settings.selectSideWindowInfo == "") {
            saveSettings({ key: "selectSideWindowInfo", value: decodeURIComponent(window_el.options[0].value) });
        }
        currentSelectWindowId = settings.selectSideWindowInfo;
        if (!set_select_checked(window_el, currentSelectWindowId)) {
            currentSelectWindowId = decodeURIComponent(window_el.value);
            saveSettings({ key: "selectSideWindowInfo", value: currentSelectWindowId });
        }
        window_el.disabled = false;
    } else {
        window_el.disabled = true;
        window_el.innerHTML = select_none;
    }
}