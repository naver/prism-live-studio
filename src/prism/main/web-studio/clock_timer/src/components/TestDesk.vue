<template>
  <div style="display: flex;align-items: center;justify-content: center;">
    <div style="width: 1300px;height: 600px;background: #1e1e1f;">
      <Viewer/>
    </div>
    <div class="setting" style="z-index:100;margin: 20px 20px;">
      <div class="settting-item">
        <span class="settting-name" style="font-family: Major Mono Display;">template00:00</span>
        <select style="margin-right: 20px;" name="type" v-model="type" id="type">
          <option value="basic">basic</option>
          <option value="round">round</option>
          <option value="flip">flip</option>
          <option value="message">message</option>
        </select>
      </div>
      <div class="settting-item">
        <div class="settting-name">type</div>
        <select style="margin-right: 20px;" name="mold" v-model="mold" id="mold">
          <option value="clock">Clock</option>
          <option value="countDown">CountDown</option>
          <option value="liveTimer">liveTimer</option>
          <option value="countUp">CountUp</option>
        </select>
      </div>
      <div class="settting-item">
        <div class="settting-name">opacity</div>
        <select style="margin-right: 20px;" name="opacity" v-model="opacity" id="opacity">
          <option value="0">0</option>
          <option value="0.3">0.3</option>
          <option value="0.5">0.5</option>
          <option value="0.7">0.7</option>
          <option value="1">1</option>
        </select>
      </div>
      <div class="settting-item">
        <div class="settting-name">fontColor</div>
        <input type="text" name="color" v-model="color" id="color">
      </div>
      <div class="settting-item">
        <div class="settting-name">background</div>
        <input type="text" name="background" v-model="background" id="background">
      </div>
      <div class="settting-item">
        <div class="settting-name">hasBackground</div>
        <input type="checkbox" name="hasBackground" v-model="hasBackground">
      </div>
      <div class="settting-item">
        <div class="settting-name">FontName</div>
        <select style="margin-right: 20px;" name="fontName" v-model="fontName" id="fontName">
          <option value="Segoe UI">Segoe UI</option>
          <option value="Agency FB">Agency FB</option>
          <option value="Major Mono Display">Major Mono Display</option>
        </select>
      </div>
      <div class="settting-item">
        <div class="settting-name">fontWeight</div>
        <select style="margin-right: 20px;" name="fontWeight" v-model="fontWeight" id="fontWeight">
          <option value="normal">normal</option>
          <option value="Italic">italic</option>
          <option value="Bold Italic">Bold Italic</option>
          <option value="lighter">lighter</option>
          <option value="bold">bold</option>
        </select>
      </div>
      <div class="settting-item">
        <div class="settting-name">countTime</div>
        <div>
          <input v-model="countTime" type="text" value="900">
        </div>
      </div>
      <div class="settting-item">
        <div class="settting-name">liveStartTime</div>
        <div>
          <input v-model="liveStartTime" type="text">
        </div>
      </div>
      <div class="settting-item">
        <div class="settting-name">Text</div>
        <input type="text" name="text" v-model="text" id="text">
      </div>
      <div class="settting-item">
        <div class="settting-name">StartText</div>
        <input type="text" name="text" v-model="startText" id="startText">
      </div>
      <div class="settting-item">
        <div class="settting-name">EndText</div>
        <input type="text" name="text" v-model="endText" id="endText">
      </div>
      <div class="settting-item">
        <div class="settting-name">endMusicName</div>
        <select style="margin-right: 20px;" name="endMusicName" v-model="endMusicName" id="endMusicName">
          <option value="1_Classic.wav">1_Classic.wav</option>
          <option value="2_Digital.wav">2_Digital.wav</option>
          <option value="3_Buzzer.wav">3_Buzzer.wav</option>
          <option value="4_Button.wav">4_Button.wav</option>
          <option value="5_Clapping.wav">5_Clapping.wav</option>
          <option value="6_Dog.wav">6_Dog.wav</option>
          <option value="7_Doorbell.wav">7_Doorbell.wav</option>
          <option value="8_Organ.wav">8_Organ.wav</option>
          <option value="9_Magic Wand.wav">9_Magic Wand.wav</option>
          <option value="10_Music Box.wav">10_Music Box.wav</option>
          <option value="11_Piano.wav">11_Piano.wav</option>
          <option value="12_Ta-da.wav">12_Ta-da.wav</option>
          <option value="13_Wave.wav">13_Wave.wav</option>
        </select>
      </div>
      <button v-if="mold!=='clock'" style="margin-left: 20px;" @click="start">start</button>
      <button v-if="mold!=='clock'" style="margin-left: 20px;" @click="pause">pause</button>
      <button v-if="mold!=='clock'" style="margin-left: 20px;" @click="resume">resume</button>
      <button v-if="mold!=='clock'" style="margin-left: 20px;" @click="cancel">cancel</button>
      <button v-if="mold!=='clock'" style="margin-left: 20px;" @click="stop">stop</button>
      <button v-if="mold!=='clock'" style="margin-left: 20px;" @click="restart">restart</button>
    </div>
  </div>
</template>
<script>
import Viewer from './Viewer'

export default {
  name: 'TestDesk',
  components: {Viewer},
  data () {
    return {
      type: 'message',
      countTime: 4,
      mold: 'clock',
      fontName: '',
      text: '현재 시간',
      color: '',
      fontWeight: 'bold',
      liveStartTime: null,
      opacity: '1',
      background: '',
      hasBackground: true,
      endMusicName: '',
      startText: 'startT',
      endText: 'endT'
    }
  },
  computed: {
    ctrlOptions () {
      return {
        liveStartTime: -1,
        countTime: -1
      }
    }
  },
  watch: {
    mold (now) {
      this.update()
    },
    text (now) {
      this.update()
    },
    opacity (now) {
      this.update()
    },
    countTime (now) {
      this.update()
    },
    liveStartTime (now) {
      this.update()
    },
    endMusicName (now) {
      this.update()
    },
    startText (now) {
      this.update()
    },
    endText (now) {
      this.update()
    },
    type: {
      handler (now) {
        this.opacity = 1
        this.liveStartTime = Date.now()
        this.endMusicName = '1_Classic.wav'
        switch (now) {
          case 'basic':
            this.fontName = 'Segoe UI'
            this.color = '#ffffff'
            this.background = '#ff8cb7'
            this.hasBackground = true
            this.fontWeight = 'bold'
            break
          case 'round':
            this.fontName = 'Major Mono Display'
            this.color = ''
            this.background = '#3c5fff'
            break
          case 'flip':
            this.fontName = 'Bebas Neue'
            this.color = '#ff4d4d'
            break
          case 'message':
            this.fontName = 'Malgun Gothic,Bebas Neue,sans-serif'
            this.background = '#df83cf, #b385e0, #a084eb, #9691e5, #80c5ee'
            this.color = '#ffffff'
            break
        }
        this.text = '현재 시간'
        this.update()
      },
      immediate: true
    },
    fontName (now) {
      this.update()
    },
    fontWeight (now) {
      this.update()
    },
    background (now) {
      this.update()
    },
    hasBackground (now) {
      this.update()
    },
    color (now) {
      this.update()
    }
  },
  mounted () {
    this.update()
  },
  methods: {
    update (reset) {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'setting',
          data: {
            type: this.type,
            mold: this.mold,
            color: this.color,
            background: this.background,
            hasBackground: this.hasBackground,
            liveStartTime: this.liveStartTime,
            fontWeight: this.fontWeight,
            opacity: this.opacity,
            countTime: this.countTime,
            fontName: this.fontName,
            endMusicName: this.endMusicName,
            text: this.text,
            startText: this.startText,
            endText: this.endText,
          }
        }
      }))
    },
    start () {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'control',
          data: {
            action: 'start',
            ...this.ctrlOptions
          }
        }
      }))
    },
    pause () {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'control',
          data: {
            action: 'pause',
            ...this.ctrlOptions
          }
        }
      }))
    },
    restart () {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'control',
          data: {
            action: 'restart',
            ...this.ctrlOptions
          }
        }
      }))
    },
    resume () {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'control',
          data: {
            action: 'resume',
            ...this.ctrlOptions
          }
        }
      }))
    },
    cancel () {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'control',
          data: {
            action: 'cancel',
            ...this.ctrlOptions
          }
        }
      }))
    },
    stop () {
      window.dispatchEvent(new CustomEvent('timerClockWidget', {
        detail: {
          type: 'control',
          data: {
            action: 'stop',
            ...this.ctrlOptions
          }
        }
      }))
    }
  }
}
</script>

<style scoped>
.settting-item {
  display: flex;
  margin-bottom: 6px;
}

.settting-name {
  margin-right: 6px;
}
</style>
