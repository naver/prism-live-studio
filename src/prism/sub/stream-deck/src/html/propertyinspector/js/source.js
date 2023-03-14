/**
 * get SceneItemId by scene Id and source Id
 * @param {string} sceneId 
 * @param {string} sourceId 
 */
function getSceneItemIdBySourceSceneId(sceneId, sourceId) {
    var result = { "sceneItemId": "", "groupSourceId": "" };
    try {
        settings['sceneList'].forEach(element => {
            if (element.id == sceneId) {
                const nodes = element.nodes;
                nodes.forEach(node => {
                    if (node.sourceId == sourceId) {
                        result = {
                            "sceneItemId": node.sceneItemId, "groupSourceId": node.parentId
                        };
                        throw new Error("EndIterative");
                    }
                });
            }
        });
    } catch (error) {
        if (error.message != "EndIterative") throw error;
    }
    return result;
}

/**
 * get scene item id by source id
 * @param {string} sourceId 
 */
function getSceneItemIdBySourceId(sourceId) {
    var result;
    try {
        settings['sceneList'].forEach(element => {
            const nodes = element.nodes;
            nodes.forEach(node => {
                if (node.sourceId == sourceId) {
                    result = {
                        "sceneItemId": node.sceneItemId, "groupSourceId": node.parentId
                    };
                    throw new Error("EndIterative");
                }
            });
        });
    } catch (error) {
        if (error.message != "EndIterative") throw error;
    }
    return result;
}

/**
 * get sources by scene id
 * @param {*} sceneId 
 */
function initializeSourceList(sceneId) {
    if (sceneId == undefined || sceneId == '')
        return;
    if (settings.hasOwnProperty('sceneList')) {
        const source_el = document.querySelector('#select_source_id');
        var parentId, parentName, elmentId, sourceId;
        const adjustSourceSelect = (arr, selectedID) => {
            return arr.map(collect => {
                if (collect.hasOwnProperty('childrenIds') && collect.childrenIds.length > 0) {
                    parentId = collect.sceneItemId;
                    parentName = collect.name;
                }
                sourceId = collect.sourceId;
                elmentId = sourceId.replace(currentCollectionID, "");
                if (collect.parentId && collect.parentId != "") {
                    elmentId = parentName + ' ' + elmentId;
                }
                return `<option value="${encodeURIComponent(sourceId)}" ${sourceId == selectedID ? 'selected' : ''}> ${elmentId}</option >`;
            }).join(' ');
        };

        /**
        * notify source select changed
        */
        source_el.onchange = () => {
            updateCurrentSourceId(decodeURIComponent(source_el.value), sceneId);
        };

        settings['sceneList'].forEach(element => {
            if (element.id == sceneId) {
                const nodes = element.nodes;
                source_el.innerHTML = adjustSourceSelect(nodes, '');
            }
        });


        if (source_el.options.length > 0) {
            set_select_checked(source_el, settings.select_source_id);
            currentSourceID = decodeURIComponent(source_el.value);
            updateCurrentSourceId(currentSourceID, sceneId);
            source_el.disabled = false;
        } else {
            updateCurrentSourceId('', '');
            source_el.disabled = true;
            source_el.innerHTML = select_none;
        }
    }
}

/**
 * request all sources inside collection id
 * @param {string} collectionId 
 */
function getSourceList(collectionId) {
    const audio_source_el = document.querySelector('#select_audio_source_id');
    audio_source_el.innerHTML = select_loading;
    // get scenes list
    const paramSource = {
        id: RPC_ID_getSources,
        method: "getSources",
        param: collectionId
    };
    sendValueToPlugin(paramSource, "data");
}

/**
 * get all the scenes and sources inside 
 * @param {string} collectionId 
 */
function getSceneAndSource(collectionId) {
    const scene_el = document.querySelector('#select_scene_id');
    const source_el = document.querySelector('#select_source_id');
    scene_el.innerHTML = select_loading;
    source_el.innerHTML = select_loading;
    // get scenes list
    const paramScene = {
        id: RPC_ID_getScenes,
        method: "getScenes",
        param: collectionId
    };
    sendValueToPlugin(paramScene, "data");
}