<template>
  <div class="viewer-item">
    <div class="view-box" :style="{'--rot':(241+rot)+'deg',...style}">
      <div class="message-box">
         <span v-for="(item,index) in showText">
          <span v-if="item !== '\\'&&item!=='\/'">{{ item }}</span>
          <span v-else style="font-family: sans-serif;">{{ item }}</span>
        </span>
      </div>
      <div class="time-box">
        <div class="hour-box group-box">
          <div class="digit-group margin-right-4" :class="groupClass">
            <MessageItem class="digit" :current="timeList[0]|transNumber"/>
            <MessageItem class="digit" :current="timeList[1]|transNumber"/>
          </div>
        </div>
        <div class="spot margin-right-4">
          <svg xmlns="http://www.w3.org/2000/svg" width="3" height="15" viewBox="0 0 3 15">
            <g fill="none" fill-rule="evenodd" opacity=".35">
              <g :fill="spotColor" fill-rule="nonzero">
                <g>
                  <g>
                    <path d="M0 0H3V3H0zM0 12H3V15H0z" transform="translate(-326 -195) translate(251 130) translate(75 65)"/>
                  </g>
                </g>
              </g>
            </g>
          </svg>

        </div>
        <div class="min-box group-box">
          <div class="digit-group margin-right-4" :class="groupClass">
            <MessageItem class="digit" :current="timeList[2]|transNumber"/>
            <MessageItem class="digit" :current="timeList[3]|transNumber"/>
          </div>
        </div>
        <div class="spot margin-right-4">
          <svg xmlns="http://www.w3.org/2000/svg" width="3" height="15" viewBox="0 0 3 15">
            <g fill="none" fill-rule="evenodd" opacity=".35">
              <g :fill="spotColor" fill-rule="nonzero">
                <g>
                  <g>
                    <path d="M0 0H3V3H0zM0 12H3V15H0z" transform="translate(-326 -195) translate(251 130) translate(75 65)"/>
                  </g>
                </g>
              </g>
            </g>
          </svg>

        </div>
        <div class="sec-box group-box">
          <div class="digit-group" :class="groupClass">
            <MessageItem class="digit" :current="timeList[4]|transNumber"/>
            <MessageItem class="digit" :current="timeList[5]|transNumber"/>
          </div>
        </div>
        <div v-if="mold==='clock'" class="zone digit-group" :class="groupClass">
          <span class="digit">{{ timeList[6] || '' }}</span>
          <span class="digit">{{ timeList[7] || '' }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { transCountTime, transClockTime } from '../../assets/js/utils'
import MessageItem from '../plugin/MessageItem'
import mixins from './mixins'

export default {
  name: 'Message',
  components: {MessageItem},
  mixins: [mixins],
  computed: {
    groupClass () {
      return this.background.toUpperCase() === '#FFFFFF' ? 'white-bg' : ''
    },
    spotColor () {
      return this.background.toUpperCase() === '#FFFFFF' ? 'rgba(0, 0, 0)' : 'rgba(255,255,255)'
    },
    style () {
      return {
        color: this.color,
        background: this.isMulBackground ? `linear-gradient(var(--rot),${this.background})` : this.background
      }
    },
    showText () {
      return (this.isEndedStatus ? this.endText : (this.mold === 'countDown' ? this.startText : this.text)).split('')
    }
  },
  data () {
    return {
      rot: 0,
      rotTimer: 0,
      rotAdd: false
    }
  },
  methods: {
    changeRot () {
      clearInterval(this.rotTimer)
      if (this.isMulBackground) {
        this.rotTimer = setInterval(() => {
          (this.rot < -90 || this.rot > 0) && (this.rotAdd = !this.rotAdd)
          this.rotAdd ? this.rot++ : this.rot--
        }, 30)
      }
    }
  }
}
</script>

<style scoped lang="scss">
.viewer-item {
  position: relative;
}

.digit-group {
  font-size: 40px;
  height: 47px;
  width: 54px;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
  border-radius: 5px;
  background-color: rgba(255, 255, 255, .35);
  position: relative;

  &.white-bg {
    background-color: rgba(204, 204, 204, .35);
  }


  .digit {
    height: 48px;
    line-height: 53px;
  }
}

.group-box {
  position: relative;
}

.hour-box::after {
  position: absolute;
  content: 'HOUR';
  opacity: 0.5;
  font-family: Bebas Neue, sans-serif;
  font-size: 11px;
  bottom: -17px;
  left: 50%;
  transform: translateX(-50%);
  margin-left: -3px;
  height: 13px;
  line-height: 13px;
}

.min-box::after {
  position: absolute;
  content: 'MINUTES';
  opacity: 0.5;
  font-family: Bebas Neue, sans-serif;
  font-size: 11px;
  bottom: -17px;
  left: 50%;
  transform: translateX(-50%);
  margin-left: -3px;
  height: 13px;
  line-height: 13px;
}

.sec-box::after {
  position: absolute;
  content: 'SECONDS';
  opacity: 0.5;
  font-family: Bebas Neue, sans-serif;
  font-size: 11px;
  bottom: -17px;
  left: 50%;
  margin-left: -3px;
  transform: translateX(-50%);
  height: 13px;
  line-height: 13px;
}

.view-box {
  --rot: 241deg;
  left: 241px;
  padding: 14px 17px 16px 17px;
  box-shadow: 0 0 6px 0 rgba(0, 0, 0, 0.4);
  border-radius: 4px;
}

.message-box {
  font-family: Malgun Gothic, Segoe UI, sans-serif;
  font-size: 16px;
  font-weight: bold;
  margin-bottom: 12px;
  max-width: 250px;
  white-space: pre-wrap;
  padding-left: 2px;
  min-height: 23px;
}

.time-box {
  font-family: Bebas Neue, sans-serif;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 3px;
  padding-bottom: 10px;
}

.margin-right-4 {
  margin-right: 4px;
}

.zone {
  margin-left: 11px;
}

.spot {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  font-size: 28px;

  .spot-dot {
    height: 10px;
    line-height: 14px;
  }
}
</style>
