<template>
  <div class="audio-play">
    <audio id="stop_audio_music" @ended="endPlay" v-if="endMusicSrc" ref="$audio" :src="endMusicSrc" preload="auto"></audio>
  </div>
</template>

<script>
import prismEvent from '../assets/js/prismEvent'
import _1 from '../assets/music/1_Classic.wav'
import _2 from '../assets/music/2_Digital.wav'
import _3 from '../assets/music/3_Buzzer.wav'
import _4 from '../assets/music/4_Button.wav'
import _5 from '../assets/music/5_Clapping.wav'
import _6 from '../assets/music/6_Dog.wav'
import _7 from '../assets/music/7_Doorbell.wav'
import _8 from '../assets/music/8_Organ.wav'
import _9 from '../assets/music/9_Magic Wand.wav'
import _10 from '../assets/music/10_Music Box.wav'
import _11 from '../assets/music/11_Piano.wav'
import _12 from '../assets/music/12_Ta-da.wav'
import _13 from '../assets/music/13_Wave.wav'

export default {
  name: 'musicPlay',
  props: {
    endMusicName: {
      type: String,
      default: ''
    }
  },
  computed: {
    endMusicSrc () {
      let index = [
        '1_Classic.wav',
        '2_Digital.wav',
        '3_Buzzer.wav',
        '4_Button.wav',
        '5_Clapping.wav',
        '6_Dog.wav',
        '7_Doorbell.wav',
        '8_Organ.wav',
        '9_Magic Wand.wav',
        '10_Music Box.wav',
        '11_Piano.wav',
        '12_Ta-da.wav',
        '13_Wave.wav'
      ].findIndex(item => item === this.endMusicName)
      return index === -1 ? false : this.musicList[index]
    }
  },
  data () {
    return {
      isPlaying: false,
      musicList: [_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13]
    }
  },
  methods: {
    play () {
      if (this.endMusicName && this.$refs.$audio) {
        this.$refs.$audio.currentTime = 0
        this.$refs.$audio.play()
        prismEvent.startMusic()
        this.isPlaying = true
      }
    },
    stop () {
      if (this.endMusicName && this.isPlaying) {
        this.$refs.$audio.pause()
        this.$refs.$audio.currentTime = 0
        prismEvent.stopMusic()
      }
      this.isPlaying = false
    },
    endPlay () {
      prismEvent.stopMusic()
    }
  }
}
</script>

<style scoped>
.audio-play {
  position: absolute;
  top: 0;
  left: 0;
  visibility: hidden;
}
</style>
