import { getFontWeight, transClockTime, transCountTime } from '../../assets/js/utils'
import { Clock, CountDown } from '../../assets/js/CountClock'
import prismEvent from '../../assets/js/prismEvent'

export default {
  props: {
    mold: {
      default: 'clock'
    },
    countTime: {
      default: 600
    },
    fontName: {
      default: 'Segoe UI'
    },
    fontWeight: {
      default: 'normal'
    },
    endMusicName: {
      default: ''
    },
    color: {
      default: '#ffffff'
    },
    liveStartTime: {
      default: Date.now()
    },
    hasBackground: {
      default: ''
    },
    background: {
      default: ''
    },
    text: {
      default: '방송 시작 전'
    },
    startText: {
      default: 'start'
    },
    endText: {
      default: 'end'
    },
    timeList: {
      default: []
    },
    isEndedStatus: {
      default: false
    }
  },
  filters: {
    transNumber (v) {
      v = +v
      return Number.isNaN(v) ? 0 : v
    }
  },
  computed: {
    style () {
      let fw = this.fontWeight.toLowerCase().trim()
      return {
        fontFamily: this.fontName,
        fontWeight: getFontWeight(fw),
        fontStyle: fw.indexOf('italic') !== -1 ? 'italic' : 'normal',
        color: this.color,
        background: this.hasBackground ? this.background : void 0
      }
    },
    isMulBackground () {
      return this.background.split(',').length > 1
    }
  },
  data () {
    return {
      spotAniClass: ''
    }
  },
  mounted () {
    this.changeRot && this.changeRot()
  },
  destroyed () {
    typeof this.rotTimer !== 'undefined' && clearInterval(this.rotTimer)
  }
}
