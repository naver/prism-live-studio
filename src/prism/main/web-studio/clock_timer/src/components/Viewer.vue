<template>
  <div class="view-container" :style="{'opacity':opacity}">
    <Flip :timeList="timeList" v-bind="propData" ref="$item" v-if="type==='flip'"/>
    <Message :timeList="timeList" v-bind="propData" ref="$item" v-else-if="type==='message'"/>
    <Round :timeList="timeList" v-bind="propData" ref="$item" v-else-if="type==='round'"/>
    <Basic :timeList="timeList" v-bind="propData" ref="$item" v-else/>
    <MusicPlay ref="$musicPlay" v-bind="propData"/>
    <ColorBar ref="$colorBar"/>
  </div>
</template>

<script>

import Flip from './types/Flip.vue'
import Message from './types/Message.vue'
import Round from './types/Round.vue'
import Basic from './types/Basic.vue'
import MusicPlay from './MusicPlay'
import ColorBar from './plugin/ColorBar'
import prismEvent from '../assets/js/prismEvent'
import { Clock, CountDown } from '../assets/js/CountClock'
import { transClockTime, transCountTime } from '../assets/js/utils'

export default {
  name: 'Viewer',
  components: {
    Flip, Message, Round, Basic, MusicPlay, ColorBar
  },
  data () {
    return {
      type: 'basic',
      mold: 'countDown',
      countTime: -1,
      fontName: '',
      fontWeight: 'normal',
      color: '',
      background: '',
      opacity: 1,
      endMusicName: '',
      hasBackground: true,
      liveStartTime: -1,
      text: '',
      startText: '',
      endText: '',
      storageOptions: null,
      storageTotalLong: null,
      timeList: new Array(8).fill(0),
      isEndedStatus: false,
      isFirstOptions: true
    }
  },
  computed: {
    propData () {
      return {
        type: this.type,
        manage: null,
        mold: this.mold,
        countTime: this.countTime,
        fontName: this.fontName,
        fontWeight: this.fontWeight,
        background: this.background,
        endMusicName: this.endMusicName,
        hasBackground: this.hasBackground,
        color: this.color,
        text: this.text,
        startText: this.startText,
        endText: this.endText,
        liveStartTime: this.liveStartTime,
        isEndedStatus: this.isEndedStatus
      }
    }
  },
  watch: {
    type () {
      this.hideBar()
    },
    mold () {
      if (this.manage && this.manage.working) {
        prismEvent.cancel()
      }
      this.createManage()
      this.isEndedStatus = false
      this.stopEndShow()
    },
    countTime (now, old) {
      this.manage.name === 'count' && this.manage.working && prismEvent.countTimeChangeWhenWorking()
      this.isEndedStatus = false
      this.createManage()
    },
    liveStartTime (now, old) {
      this.manage.name === 'count' && this.manage.working && prismEvent.liveStartTimeChangeWhenWorking()
      this.isEndedStatus = false
    },
    propData (now, pre) {
      let result = {}
      for (let key in pre) {
        pre[key] !== now[key] && (result[key] = {now: now[key], pre: pre[key]})
      }
      Object.keys(result).length && (this.printLog({message: 'timer options update', data: result}))
    }
  },
  methods: {
    handleEvent (option) {
      option = option.detail || option
      if (typeof option !== 'object') {
        try {
          option = JSON.parse(option)
        } catch (e) {
          return this.printLog({message: 'An error occurred while parsing the timer data', data: e})
        }
      }
      if (option.type === 'setting') {
        let {data} = option
        if (this.type !== data.type) {
          this.type = data.type
        }
        data.mold && (this.mold = data.mold)
        notUndefined(data.fontName) && (this.fontName = data.fontName)
        notUndefined(data.fontWeight) && (this.fontWeight = data.fontWeight)
        notUndefined(data.opacity) && (this.opacity = data.opacity)
        notUndefined(data.color) && (this.color = data.color)
        notUndefined(data.background) && (this.background = data.background)
        notUndefined(data.endMusicName) && (this.endMusicName = data.endMusicName)
        notUndefined(data.text) && (this.text = data.text)
        notUndefined(data.startText) && (this.startText = data.startText)
        notUndefined(data.endText) && (this.endText = data.endText)
        notUndefined(data.countTime) && (this.countTime = +data.countTime)
        notUndefined(data.liveStartTime) && (this.liveStartTime = +data.liveStartTime)
        notUndefined(data.hasBackground) && (this.hasBackground = data.hasBackground)
      } else if (option.type === 'control') {
        let {data} = option
        notUndefined(data.countTime) && data.countTime !== -1 && (this.countTime = +data.countTime)
        notUndefined(data.liveStartTime) && data.liveStartTime !== -1 && (this.liveStartTime = +data.liveStartTime)
        this.handleControl(data)
      }
      this.$nextTick(() => {
        this.isFirstOptions && prismEvent.handleFirstOption()
        this.isFirstOptions = false
      })
    },
    handleControl (data) {
      if (!this.$refs.$item) return
      this.isEndedStatus = false
      this.$nextTick(() => {
        switch (data.action) {
          case 'start':
            this.mold === 'liveTimer' ? this.createManage().start() : this.manage.start()
            this.printLog({message: 'Received a command to start the timer'})
            prismEvent.start()
            break
          case 'stop':
            if (this.mold === 'liveTimer') {
              this.manage.reset()
              this.mold === 'liveTimer' && (this.timeList = transCountTime(0))
              this.liveStartTime = -1
            } else {
              this.manage && this.manage.pause()
            }
            this.stopEndShow()
            this.printLog({message: 'Received stop timer command'})
            prismEvent.stop()
            break
          case 'pause':
            this.manage && this.manage.pause()
            this.printLog({message: 'Received a command to pause the timer'})
            prismEvent.pause()
            break
          case 'resume':
            this.manage && this.manage.resume()
            this.printLog({message: 'Received the command to resume the timer'})
            prismEvent.resume()
            break
          case 'restart':
            if (this.mold === 'liveTimer') {
              this.createManage().start()
            } else {
              this.manage && this.manage.reset()
              this.manage.start()
            }
            this.stopEndShow()
            this.printLog({message: 'Received the command to restart the timer'})
            break
          case 'cancel':
            if (this.mold === 'countUp') {
              this.manage.totalLong = 0
            }
            this.manage.reset()
            this.stopEndShow()
            this.printLog({message: 'Received a command to cancel or reset the timer'})
            prismEvent.cancel()
            break
        }
      })
    },
    clearManage () {
      if (this.manage) {
        this.manage.end()
        this.manage.reset()
        this.manage = null
      }
      this.timeList = new Array(8).fill(0)
    },
    createManage () {
      this.clearManage()
      if (this.mold === 'countDown' || this.mold === 'countUp') {
        this.manage = new CountDown({
          mold: this.mold === 'countDown' ? 'down' : 'up',
          countTime: this.countTime === -1 ? 0 : this.countTime,
          processhandle: time => {
            this.timeList = transCountTime(time)
            this.itemAction('spotAni')
          },
          endHandle: (isNature) => {
            this.countUpDownWorking = false
            if (isNature) {
              prismEvent.finish()
              if (!this.endMusicName) {
                prismEvent.stopMusic()
              } else {
                this.$refs.$musicPlay.play()
              }
              this.showBar()
              this.isEndedStatus = true
            }
          }
        })
      } else if (this.mold === 'clock') {
        this.manage = new Clock({
          processhandle: time => {
            this.itemAction('spotAni')
            this.timeList = transClockTime(time)
          },
          endHandle: () => {
            this.manage.reset()
          }
        })
      } else if (this.mold === 'liveTimer') {
        let liveTimeStart = this.liveStartTime === -1 ? 0 : Math.floor((Date.now() - this.liveStartTime) / 1000)
        this.manage = new CountDown({
          mold: 'up',
          countTime: liveTimeStart,
          processhandle: time => {
            this.timeList = transCountTime(time)
            this.itemAction('spotAni')
          }
        })
        this.printLog({message: 'liveTimer startTime countTime', liveTimeStart})
        this.$nextTick(() => {
          this.timeList = transCountTime(0)
        })
      }
      return this.manage
    },
    getLiveStartSeconds () {
      if (+this.liveStartTime === -1) {
        return 0
      } else {
        return Math.floor((Date.now() - this.liveStartTime) / 1000)
      }
    },
    itemAction (actionName) {
      if (!this.$refs.$item) return
      typeof this.$refs.$item[actionName] === 'function' && this.$refs.$item[actionName]()
    },
    stopEndShow () {
      this.$refs.$musicPlay.stop()
      this.hideBar()
    },
    showBar () {
      this.type === 'message' && this.$refs.$colorBar && this.$refs.$colorBar.showBar()
    },
    hideBar () {
      this.$refs.$colorBar && this.$refs.$colorBar.hideBar()
    }
  },
  mounted () {
    window.addEventListener('timerClockWidget', this.handleEvent)
    window.injectEvent = this.handleEvent
    this.createManage()
    this.$nextTick(() => {
      prismEvent.pageLoaded()
    })
  }
}

function notUndefined (type) {
  return typeof type !== 'undefined'
}
</script>

<style scoped>
.view-container {
  width: 100%;
  height: 100%;
  color: white;
  overflow: hidden;
  font-family: SegoeUI;
  font-size: 50px;
  z-index: 1;
  position: relative;
}
</style>
