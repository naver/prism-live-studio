/* global addDynamicStyles, $SD, Utils */
/* eslint-disable no-extra-boolean-cast */
/* eslint-disable no-else-return */

/**
 * This example contains a working Property Inspector, which already communicates
 * with the corresponding plugin throug settings and/or direct messages.
 * If you want to use other control-types, we recommend copy/paste these from the
 * PISamples demo-library, which already contains quite some example DOM elements 
 */


/**
 * First we declare a global variable, which change all elements behaviour
 * globally. It installs the 'onchange' or 'oninput' event on the HTML controls and fiels.
 * 
 * Change this, if you want interactive elements act on any modification (oninput),
 * or while their value changes 'onchange'.
 */

var onchangeevt = 'onchange'; // 'oninput'; 

// RPC ID
const RPC_ID_startStreaming = 1;
const RPC_ID_stopStreaming = 2;
const RPC_ID_startRecording = 3;
const RPC_ID_stopRecording = 4;
//const RPC_ID_getCollections = 5;
const RPC_ID_makeCollectionActive = 6;
const RPC_ID_fetchSceneCollectionsSchema = 8;
const RPC_ID_getScenes = 9;
const RPC_ID_getSources = 10;
const RPC_ID_makeSceneActive = 11;
const RPC_ID_getActiveSceneId = 12;
const RPC_ID_muteMixerAudioSource = 13;
const RPC_ID_unmuteMixerAudioSource = 14;
const RPC_ID_hideScene = 15;
const RPC_ID_showScene = 16;
const RPC_ID_subscribeToSceneSwitched = 17;
const RPC_ID_subscribeToSceneAdded = 18;
const RPC_ID_subscribeToSceneRemoved = 19;
const RPC_ID_subscribeToSouceAdded = 20;
const RPC_ID_subscribeToSourceRemoved = 21;
const RPC_ID_subscribeToSourceUpdated = 22;
const RPC_ID_subscribeToItemAdded = 23;
const RPC_ID_subscribeToItemRemoved = 24;
const RPC_ID_subscribeToItemUpdated = 25;
const RPC_ID_subscribeToStreamingStatusChanged = 26;
const RPC_ID_getActiveCollection = 27;
const RPC_ID_subscribeToCollectionAdded = 28;
const RPC_ID_subscribeToCollectionRemoved = 29;
const RPC_ID_subscribeToCollectionSwitched = 30;
const RPC_ID_getRecordingAndStreamingState = 31;
const RPC_ID_subscribeToCollectionUpdated = 32;
const RPC_ID_subscribeToRecordingStatusChanged = 33;
const RPC_ID_setPushToProgramInStudioMode = 34;
const RPC_ID_showSideWindow = 35;
const RPC_ID_muteAll = 36;
const RPC_ID_unmuteAll = 37;
const RPC_ID_getMasterMuteState = 38;
const RPC_ID_getSideWindowInfo = 39;
const RPC_ID_updateSceneList = 1000;
const RPC_ID_updateSourceList = 1001;
const RPC_ID_getPrismConnectionState = 1002;
const RPC_ID_prismLoadingFinished = 1003;

// events
const EVENT_STREAMING_STATUS_CHANGED = "StreamingService.streamingStatusChange"
const EVENT_RECORDING_STATUS_CHANGED = "StreamingService.recordingStatusChange"
const EVENT_SCENE_SWITCHED = "ScenesService.sceneSwitched"
const EVENT_SCENE_ADDED = "ScenesService.sceneAdded"
const EVENT_SOURCE_UPDATED = "SourcesService.sourceUpdated"
const EVENT_SCENECOLLECTION_SWITCHED = "SceneCollectionsService.collectionSwitched"
const EVENT_SCENECOLLECTION_ADDED = "SceneCollectionsService.collectionAdded"
const EVENT_STUDIOMODE_STATUS_CHANGED = "StudioModeService.studioModeStatusChange"
const EVENT_MUTE_ALL_STATUS_CHANGED = "Mainview.audioMixerMuteChanged"
const EVENT_ITEM_UPDATED = "ScenesService.itemUpdated"
const EVENT_SIDE_WINDOW_VISIBLE_CHANGED = "Mainview.sideWindowVisibleChanged"


// status
const STATUS_STARTING = "starting"
const STATUS_RECORDING = "recording"
const STATUS_STOPPING = "stopping"
const STATUS_OFFLINE = "offline"

/**
 * common select option element
 */
let select_none
let select_loading

/**
 * cache the static SDPI-WRAPPER, which contains all your HTML elements.
 * Please make sure, you put all HTML-elemenets into this wrapper, so they
 * are drawn properly using the integrated CSS.
 */

let sdpiWrapper = document.querySelector('.sdpi-wrapper');

/**
 * Since the Property Inspector is instantiated every time you select a key
 * in Stream Deck software, we can savely cache our settings in a global variable.
 */

let settings;

/**
 * Since the Property Inspector is instantiated every time you select a key
 * in Stream Deck software, we can savely cache the action type in a global variable.
 */

let actionType;

/**
 * The currently collection ID
 */

var currentCollectionID;

/**
 * The currently scene ID
 */

var currentSceneID;

/**
 * The currently source ID
 */

var currentSourceID;

/**
 * The currently audio source id
 */

var currentAudioSourceId;

/**
 * The currently select window id
 */
var currentSelectWindowId;



let context;


var collections = {};

/**
 * The 'connected' event is the first event sent to Property Inspector, after it's instance
 * is registered with Stream Deck software. It carries the current websocket, settings,
 * and other information about the current environmet in a JSON object.
 * You can use it to subscribe to events you want to use in your plugin.
 */

/**
 * 
 * scene collection
 * scene list
 * source list
 */
var sceneCollection = null;
var sceneList = null;
var sourceList = null;

var isConnected = false;

$SD.on('connected', (jsn) => {
    /**
     * The passed 'applicationInfo' object contains various information about your
     * computer, Stream Deck version and OS-settings (e.g. colors as set in your
     * OSes display preferences.)
     * We use this to inject some dynamic CSS values (saved in 'common_pi.js'), to allow
     * drawing proper highlight-colors or progressbars.
     */

    console.log("connected");
    console.log("current json:", jsn);
    addDynamicStyles($SD.applicationInfo.colors, 'connectSocket');

    /**
     * Current settings are passed in the JSON node
     * {actionInfo: {
     *      payload: {
     *          settings: {
     *                  yoursetting: yourvalue,
     *                  otherthings: othervalues
     * ...
     * To conveniently read those settings, we have a little utility to read
     * arbitrary values from a JSON object, eg:
     * 
     * const foundObject = Utils.getProp(JSON-OBJECT, 'path.to.target', defaultValueIfNotFound)
     */
    context = Utils.getProp(jsn, 'actionInfo.context', '');

    select_none = `<option value=\"none\">${'NoneOption'.lox()}</option>`;
    select_loading = `<option value=\"none\">${'Loading'.lox()}</option>`;

    // remember settings
    settings = Utils.getProp(jsn, 'actionInfo.payload.settings', false);
    if (settings) {
        if (settings.hasOwnProperty('currentCollectionID')) {
            currentCollectionID = settings['currentCollectionID'];
        } else {
            currentCollectionID = "";
        }

        if (settings.hasOwnProperty('currentSceneID')) {
            currentSceneID = settings['currentSceneID'];
        } else {
            currentSceneID = "";
        }

        if (settings.hasOwnProperty('currentSourceID')) {
            currentSourceID = settings['currentSourceID'];
        } else {
            currentSourceID = "";
        }

        if (settings.hasOwnProperty('collectionList')) {
            sceneCollection = settings['collectionList'];
        }

        if (settings.hasOwnProperty('sceneList')) {
            sceneList = settings['sceneList'];
        }

        if (settings.hasOwnProperty('sourceList')) {
            sourceList = settings['sourceList'];
        }

        updateUI(settings);
    }

    actionType = Utils.getProp(jsn, 'actionInfo.action', false);
    if (actionType) {
        const collection_label = document.querySelector("#select_collection_label");
        const scene_label = document.querySelector("#select_scene_label");
        const source_label = document.querySelector("#select_source_label");
        const window_label = document.querySelector("#select_side_window_label");
        if (actionType == "com.naver.streamdeck.pls.source") {
            source_label.innerText = 'Source'.lox();
            scene_label.innerText = 'Scene'.lox();
            collection_label.innerText = 'Collection'.lox();
        } else if (actionType == "com.naver.streamdeck.pls.scene" || actionType == "com.naver.streamdeck.pls.mixeraudio") {
            collection_label.innerText = 'Collection'.lox();
            scene_label.innerText = 'Scene'.lox();
        } else if (actionType == "com.naver.streamdeck.pls.sidewindow") {
            window_label.innerText = 'Window'.lox();
        }

        const collectionCom = document.querySelector("#select_collection");
        const sceneCom = document.querySelector("#select_scene");
        const sourceCom = document.querySelector("#select_source");
        const sourceAudioCom = document.querySelector("#select_audio_source");
        const sideWindow = document.querySelector("#select_side_window");
        const piDescribe = document.querySelector("#property_inspector_tip_id");

        const hideAllElement = () => {
            setElmentVisible(collectionCom, false);
            setElmentVisible(sourceCom, false);
            setElmentVisible(sceneCom, false);
            setElmentVisible(sideWindow, false);
            setElmentVisible(sourceAudioCom, false);
        };

        if (actionType == "com.naver.streamdeck.pls.source") {
            setElmentVisible(collectionCom, true);
            setElmentVisible(sceneCom, true);
            setElmentVisible(sourceCom, true);
            setElmentVisible(sourceAudioCom, false);
            setElmentVisible(sideWindow, false);
            setElmentVisible(piDescribe, false);
        } else if (actionType == "com.naver.streamdeck.pls.scene") {
            setElmentVisible(collectionCom, true);
            setElmentVisible(sceneCom, true);
            setElmentVisible(sourceCom, false);
            setElmentVisible(sourceAudioCom, false);
            setElmentVisible(sideWindow, false);
            setElmentVisible(piDescribe, false);
        } else if (actionType == "com.naver.streamdeck.pls.mixeraudio") {
            setElmentVisible(collectionCom, true);
            setElmentVisible(sourceAudioCom, true);
            setElmentVisible(sceneCom, false);
            setElmentVisible(sourceCom, false);
            setElmentVisible(sideWindow, false);
            setElmentVisible(piDescribe, false);
        } else if (actionType == "com.naver.streamdeck.pls.sidewindow") {
            setElmentVisible(collectionCom, false);
            setElmentVisible(sourceCom, false);
            setElmentVisible(sourceAudioCom, false);
            setElmentVisible(sceneCom, false);
            setElmentVisible(sideWindow, true);
            setElmentVisible(piDescribe, false);
            /**
             * init side windows list
             */
            getSideWindowInfo();
        } else if (actionType == "com.naver.streamdeck.pls.cpuusage") {
            hideAllElement();
            piDescribe.innerText = 'CPUUsagePiTip'.lox();
        } else if (actionType == "com.naver.streamdeck.pls.framedrop") {
            hideAllElement();
            piDescribe.innerText = 'FramedropPiTip'.lox();
        } else if (actionType == "com.naver.streamdeck.pls.bitrate") {
            hideAllElement();
            piDescribe.innerText = 'BitratePiTip'.lox();
        } else {
            hideAllElement();
            setElmentVisible(piDescribe, false);
            piDescribe.innerText = "";
        }

        /**
         * initialize connection state of prism
         */
        getPrismConnectionState();
    }
});

function setElmentVisible(element, visible) {
    element.style.visibility = visible ? "visible" : "hidden";
    element.style.display = visible ? "flex" : "none";
}

/**
 * get side bar windows' info
 */
function getSideWindowInfo() {
    const param = {
        id: RPC_ID_getSideWindowInfo,
        param: "WindowsService"
    };
    sendValueToPlugin(param, "data");
}

/**
* initialize connection state of prism
*/
function getPrismConnectionState() {
    const param = {
        id: RPC_ID_getPrismConnectionState,
        param: "WindowsService"
    };
    sendValueToPlugin(param, "data");
}

/**
 * The 'sendToPropertyInspector' event can be used to send messages directly from your plugin
 * to the Property Inspector without saving these messages to the settings.
 */

$SD.on('sendToPropertyInspector', (jsn) => messageFromPlugin(jsn));


// handle message from plugin
function messageFromPlugin(jsn) {
    console.log("sendToPropertyInspector called");
    console.log("json content:", jsn);

    if (jsn.event == "sendToPropertyInspector") {
        // handle events from plugin
        handleEventsFromPlugin(jsn);
    }

}

// update UI with message from plugin
function updatePropertyInspectorUi(isConnected) {
    const collection_el = document.querySelector('#select_collection_id');
    const scene_el = document.querySelector('#select_scene_id');
    const source_el = document.querySelector('#select_source_id');
    const source_audio_el = document.querySelector('#select_audio_source_id');
    const window_el = document.querySelector('#select_side_window_id');
    const warningEl = document.querySelector("#please_launch_warning");
    const piDescribe = document.querySelector("#property_inspector_tip_id");

    if (!isConnected) {
        warningEl.style.visibility = "visible";
        console.log("please_launch_warning show");

        // clear the lists
        collection_el.innerHTML = select_none;
        scene_el.innerHTML = select_none;
        source_el.innerHTML = select_none;
        source_audio_el.innerHTML = select_none;
        window_el.innerHTML = select_none;

        collection_el.disabled = true;
        scene_el.disabled = true;
        source_el.disabled = true;
        source_audio_el.disabled = true;
        window_el.disabled = true;
    } else {
        warningEl.style.visibility = "hidden";
        console.log("please_launch_warning hide");

        const judgeRequest = () => {
            if (undefined == settings.collectionList) {
                getSceneCollection();
            } else {
                initializeCollectionList(settings.collectionList);
            }
        };

        if (actionType == "com.naver.streamdeck.pls.source") {
            judgeRequest();
            piDescribe.innerText = 'SourcePiTip'.lox();
        } else if (actionType == "com.naver.streamdeck.pls.scene") {
            judgeRequest();

        } else if (actionType == "com.naver.streamdeck.pls.mixeraudio") {
            judgeRequest();
        } else if (actionType == "com.naver.streamdeck.pls.sidewindow") {

            /**
             * init side windows list
             */
            getSideWindowInfo();
        }
    }
}


$SD.on('didReceiveSettings', jsn => {
    console.log("didReceiveSettings called");
    if (jsn.payload && jsn.payload.hasOwnProperty('payload')) {
        handleDidReceiveSettings(jsn.payload);
    }
});

const updateUI = (pl) => {
    Object.keys(pl).map(e => {
        if (e && e != '') {
            const foundElement = document.querySelector(`#${e}`);
            console.log(`searching for: #${e}`, 'found:', foundElement);
            if (foundElement && foundElement.type !== 'file') {
                foundElement.value = pl[e];
                const maxl = foundElement.getAttribute('maxlength') || 50;
                const labels = document.querySelectorAll(`[for='${foundElement.id}']`);
                if (labels.length) {
                    for (let x of labels) {
                        x.textContent = maxl ? `${foundElement.value.length}/${maxl}` : `${foundElement.value.length}`;
                    }
                }
            }
        }
    })
}

/**
 * 
 * @param {selector} selector 
 * @param {value that you want to select} checkValue 
 */
function set_select_checked(selector, checkValue) {

    for (var i = 0; i < selector.options.length; i++) {
        if (selector.options[i].value == encodeURIComponent(checkValue)) {
            selector.options[i].selected = true;
            return true;
        }
    }

    if (selector.options.length > 0) {
        selector.options[0].selected = true;
    }
    return false;
}

/**
 * Something in the PI changed:
 * either you clicked a button, dragged a slider or entered some text
 * 
 * The 'piDataChanged' event is sent, if data-changes are detected.
 * The changed data are collected in a JSON structure
 * 
 * It looks like this:
 * 
 *  {
 *      checked: false
 *      group: false
 *      index: 0
 *      key: "mynameinput"
 *      selection: []
 *      value: "Elgato"
 *  } 
 * 
 * If you set an 'id' to an input-element, this will get the 'key' of this object.
 * The input's value will get the value.
 * There are other fields (e.g. 
 *      - 'checked' if you clicked a checkbox
 *      - 'index', if you clicked an element within a group of other elements
 *      - 'selection', if the element allows multiple-selections
 * ) 
 * 
 * Please note: 
 * the template creates this object for the most common HTML input-controls.
 * This is a convenient way to start interacting with your plugin quickly.
 * 
 */

$SD.on('piDataChanged', (returnValue) => {

    console.log('%c%s', 'color: white; background: blue}; font-size: 15px;', 'piDataChanged');
    console.log(returnValue);

    // /* SAVE THE VALUE TO SETTINGS */
    saveSettings(returnValue);

    /* SEND THE VALUES TO PLUGIN */
    sendValueToPlugin(returnValue, 'sdpi_collection');
});

/**
 * 'sendValueToPlugin' is a wrapper to send some values to the plugin
 * 
 * It is called with a value and the name of a property:
 * 
 * sendValueToPlugin(<any value>), 'key-property')
 * 
 * where 'key-property' is the property you listen for in your plugin's
 * 'sendToPlugin' events payload.
 * 
 */

function sendValueToPlugin(value, prop) {
    console.log("sendValueToPlugin", value, prop);
    if ($SD.connection && $SD.connection.readyState === 1) {
        const json = {
            action: $SD.actionInfo['action'],
            event: 'sendToPlugin',
            context: $SD.uuid,
            payload: {
                [prop]: value,
                targetContext: $SD.actionInfo['context']
            }
        };

        $SD.connection.send(JSON.stringify(json));
    }
}

/** CREATE INTERACTIVE HTML-DOM
 * The 'prepareDOMElements' helper is called, to install events on all kinds of
 * elements (as seen e.g. in PISamples)
 * Elements can get clicked or act on their 'change' or 'input' event. (see at the top
 * of this file)
 * Messages are then processed using the 'handleSdpiItemChange' method below.
 * If you use common elements, you don't need to touch these helpers. Just take care
 * setting an 'id' on the element's input-control from which you want to get value(s).
 * These helpers allow you to quickly start experimenting and exchanging values with
 * your plugin.
 */

function prepareDOMElements(baseElement) {
    baseElement = baseElement || document;
    Array.from(baseElement.querySelectorAll('.sdpi-item-value')).forEach(
        (el, i) => {
            const elementsToClick = [
                'BUTTON',
                'OL',
                'UL',
                'TABLE',
                'METER',
                'PROGRESS',
                'CANVAS'
            ].includes(el.tagName);
            const evt = elementsToClick ? 'onclick' : onchangeevt || 'onchange';

            /** Look for <input><span> combinations, where we consider the span as label for the input
             * we don't use `labels` for that, because a range could have 2 labels.
             */
            const inputGroup = el.querySelectorAll('input + span');
            if (inputGroup.length === 2) {
                const offs = inputGroup[0].tagName === 'INPUT' ? 1 : 0;
                inputGroup[offs].textContent = inputGroup[1 - offs].value;
                inputGroup[1 - offs]['oninput'] = function () {
                    inputGroup[offs].textContent = inputGroup[1 - offs].value;
                };
            }
            /** We look for elements which have an 'clickable' attribute
             * we use these e.g. on an 'inputGroup' (<span><input type="range"><span>) to adjust the value of
             * the corresponding range-control
             */
            Array.from(el.querySelectorAll('.clickable')).forEach(
                (subel, subi) => {
                    subel['onclick'] = function (e) {
                        handleSdpiItemChange(e.target, subi);
                    };
                }
            );
            /** Just in case the found HTML element already has an input or change - event attached, 
             * we clone it, and call it in the callback, right before the freshly attached event
            */
            const cloneEvt = el[evt];
            el[evt] = function (e) {
                if (cloneEvt) cloneEvt();
                handleSdpiItemChange(e.target, i);
            };
        }
    );

    /**
     * You could add a 'label' to a textares, e.g. to show the number of charactes already typed
     * or contained in the textarea. This helper updates this label for you.
     */
    baseElement.querySelectorAll('textarea').forEach((e) => {
        const maxl = e.getAttribute('maxlength');
        e.targets = baseElement.querySelectorAll(`[for='${e.id}']`);
        if (e.targets.length) {
            let fn = () => {
                for (let x of e.targets) {
                    x.textContent = maxl ? `${e.value.length}/${maxl}` : `${e.value.length}`;
                }
            };
            fn();
            e.onkeyup = fn;
        }
    });

    baseElement.querySelectorAll('[data-open-url').forEach(e => {
        const value = e.getAttribute('data-open-url');
        if (value) {
            e.onclick = () => {
                let path;
                if (value.indexOf('http') !== 0) {
                    path = document.location.href.split('/');
                    path.pop();
                    path.push(value.split('/').pop());
                    path = path.join('/');
                } else {
                    path = value;
                }
                $SD.api.openUrl($SD.uuid, path);
            };
        } else {
            console.log(`${value} is not a supported url`);
        }
    });
}

function handleSdpiItemChange(e, idx) {

    /** Following items are containers, so we won't handle clicks on them */

    if (['OL', 'UL', 'TABLE'].includes(e.tagName)) {
        return;
    }

    /** SPANS are used inside a control as 'labels'
     * If a SPAN element calls this function, it has a class of 'clickable' set and is thereby handled as
     * clickable label.
     */

    if (e.tagName === 'SPAN') {
        const inp = e.parentNode.querySelector('input');
        if (inp && e.hasAttribute('value')) {
            inp.value = e.getAttribute('value');
        } else return;
    }

    const selectedElements = [];
    const isList = ['LI', 'OL', 'UL', 'DL', 'TD'].includes(e.tagName);
    const sdpiItem = e.closest('.sdpi-item');
    const sdpiItemGroup = e.closest('.sdpi-item-group');
    let sdpiItemChildren = isList
        ? sdpiItem.querySelectorAll(e.tagName === 'LI' ? 'li' : 'td')
        : sdpiItem.querySelectorAll('.sdpi-item-child > input');

    if (isList) {
        const siv = e.closest('.sdpi-item-value');
        if (!siv.classList.contains('multi-select')) {
            for (let x of sdpiItemChildren) x.classList.remove('selected');
        }
        if (!siv.classList.contains('no-select')) {
            e.classList.toggle('selected');
        }
    }

    if (sdpiItemChildren.length && ['radio', 'checkbox'].includes(sdpiItemChildren[0].type)) {
        e.setAttribute('_value', e.checked); //'_value' has priority over .value
    }
    if (sdpiItemGroup && !sdpiItemChildren.length) {
        for (let x of ['input', 'meter', 'progress']) {
            sdpiItemChildren = sdpiItemGroup.querySelectorAll(x);
            if (sdpiItemChildren.length) break;
        }
    }

    if (e.selectedIndex) {
        idx = e.selectedIndex;
    } else {
        sdpiItemChildren.forEach((ec, i) => {
            if (ec.classList.contains('selected')) {
                selectedElements.push(ec.textContent);
            }
            if (ec === e) {
                idx = i;
                selectedElements.push(ec.value);
            }
        });
    }

    const returnValue = {
        key: e.id && e.id.charAt(0) !== '_' ? e.id : sdpiItem.id,
        value: isList
            ? e.textContent
            : e.hasAttribute('_value')
                ? e.getAttribute('_value')
                : e.value
                    ? e.type === 'file'
                        ? decodeURIComponent(e.value.replace(/^C:\\fakepath\\/, ''))
                        : e.value
                    : e.getAttribute('value'),
        group: sdpiItemGroup ? sdpiItemGroup.id : false,
        index: idx,
        selection: selectedElements,
        checked: e.checked
    };

    /** Just simulate the original file-selector:
     * If there's an element of class '.sdpi-file-info'
     * show the filename there
     */
    if (e.type === 'file') {
        const info = sdpiItem.querySelector('.sdpi-file-info');
        if (info) {
            const s = returnValue.value.split('/').pop();
            info.textContent = s.length > 28
                ? s.substr(0, 10)
                + '...'
                + s.substr(s.length - 10, s.length)
                : s;
        }
    }

    $SD.emit('piDataChanged', returnValue);
}

/**
 * This is a quick and simple way to localize elements and labels in the Property
 * Inspector's UI without touching their values.
 * It uses a quick 'lox()' function, which reads the strings from a global
 * variable 'localizedStrings' (in 'common.js')
 */

// eslint-disable-next-line no-unused-vars
function localizeUI() {
    const el = document.querySelector('.sdpi-wrapper') || document;
    let t;
    Array.from(el.querySelectorAll('sdpi-item-label')).forEach(e => {
        t = e.textContent.lox();
        if (e !== t) {
            e.innerHTML = e.innerHTML.replace(e.textContent, t);
        }
    });
    Array.from(el.querySelectorAll('*:not(script)')).forEach(e => {
        if (
            e.childNodes
            && e.childNodes.length > 0
            && e.childNodes[0].nodeValue
            && typeof e.childNodes[0].nodeValue === 'string'
        ) {
            t = e.childNodes[0].nodeValue.lox();
            if (e.childNodes[0].nodeValue !== t) {
                e.childNodes[0].nodeValue = t;
            }
        }
    });
}

/** 
 *
 * get Scene Collection
 *  
*/
function getSceneCollection() {
    const json = {
        id: RPC_ID_fetchSceneCollectionsSchema,
        params: {
            resource: "SceneCollectionsService",
            method: "fetchSceneCollectionsSchema"
        }
    };
    sendValueToPlugin(json, "data");
}

/**
 * 
 * Some more (de-) initialization helpers 
 * 
 */

document.addEventListener('DOMContentLoaded', function () {
    document.body.classList.add(navigator.userAgent.includes("Mac") ? 'mac' : 'win');
    prepareDOMElements();
    $SD.on('localizationLoaded', (language) => {
        localizeUI();
    });
});

/** the beforeunload event is fired, right before the PI will remove all nodes */
window.addEventListener('beforeunload', function (e) {
    e.preventDefault();
    sendValueToPlugin('propertyInspectorWillDisappear', 'property_inspector');
    // Don't set a returnValue to the event, otherwise Chromium with throw an error.  // e.returnValue = '';
});