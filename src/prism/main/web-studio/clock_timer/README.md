# timerClockWidget

## URL

URL: /data/timer_clock/index.html

## setting example

```js
dispatchEvent(new CustomEvent("timerClockWidget", {
  detail: {
    type: 'setting',
    data: {
      type: this.type,
      mold: this.mold,
      color: this.color,
      background: this.background,
      hasBackground: this.hasBackground,
      opacity: this.opacity,
      countTime: this.countTime,
      liveStartTime: this.liveStartTime,
      fontName: this.fontName,
      fontWeight: this.fontWeight,
      endMusicName: this.endMusicName,
      text: this.text
    }
  }))
```

|  name   | type | necessary | default |describe|
| --- | ---  | --- | --- | --- |
|  type | String | true | null |basic,round, flip, message|
|  mold | String | true |null|clock(当前时间),liveTimer(直播时间),countDown（倒计时）,countUp(正计时)|
|  color | String | false |‘’| 字体颜色 |
|  background | String | false |‘’| 背景颜色 ,当type为message时支持多个背景色，有多个背景色时使用‘,’逗号隔开，如‘#df83cf, #b385e0, #a084eb, #9691e5, #80c5ee’|
|  hasBackground | String | false |‘’| 背景on/off ,当type为basic时支持|
|  opacity | String | false |‘’| 0-1.0 透明度|
|  countTime | Number | false |0| type为countDown（倒计时）或countUp(正计时)，计时起点，如为60时，就是60秒开始倒计时到0，或者从60秒正计时增长，没有或为空时传-1|
|  liveStartTime | String | false |‘’| 直播开始时间，如1623117453805，没有或为空时传-1|
|  fontName | String | false |‘’| 字体名称，type为basic时支持更换字体|
|  fontWeight | String | false |‘’| 字体大小，type为basic时支持更换字体大小|
|  endMusicName | String | false |‘’| 结束音乐名称，倒计时结束时使用的音乐|
|  text | String | false |‘’| type为message时的text信息|
|  startText | String | false |‘’| type为message时的开始直播text信息|
|  endText | String | false |‘’| type为message时的结束text信息|

## prism控制计时器的参数

mold为liveTimer(直播时间),countDown（倒计时）,countUp(正计时)时使用 发送此消息时开始计时，liveTimer，countDown，countUp。在此处可以补充或者修改countTime和liveStartTime

```js
window.dispatchEvent(new CustomEvent('timerClockWidget', {
  detail: {
    type: 'control',
    data: {
      action: 'start',
      liveStartTime: -1,
      countTime: -1,
    }
  }
}))
```

|  name   | type | necessary | default |describe|
| --- | ---  | --- | --- | --- |
|  type | String | true | null |固定为control|
|  action | String | true | null |start,pause,stop,restart,cancel|
|  liveStartTime | String | true | null |直播开始时间，没有或为空时传-1|
|  countTime | String | true | null |正倒计时基础时间，没有或为空时传-1|

## 计时器发送给Prism的消息

```js
`{
  module: 'timerClockWidget',
  data: {
    eventName: eventName
}`
```
eventName：

- start 计时器开始
- finish 计时器自然结束
- pause  计时器被暂停
- resume  计时器被暂停后继续计时
- restart  计时器重置且重新计时
- cancel  计时器被重置且不及时
- stop   计时器停止
- startMusic  音乐开始播放
- stopMusic  音乐停止播放
- countTimeChangeWhenWorking  正在计时时，countTime被改变
- liveStartTimeChangeWhenWorking  正在计时时，liveStartTime被改变
