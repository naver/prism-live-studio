<template>
  <div class="viewer-item">
    <div class="time-box">
      <flip-item :lineColor="lineColor" :styles="style" :total="9" :current="+timeList[0]"/>
      <flip-item :lineColor="lineColor" :styles="style" :total="9" :current="+timeList[1]"/>
      <div class="colon">
        <svg xmlns="http://www.w3.org/2000/svg" width="5" height="23" viewBox="0 0 5 23">
          <g fill="none" fill-rule="evenodd">
            <g :fill="colonColor" fill-rule="nonzero">
              <g>
                <g>
                  <path d="M5 5L5 0 0 0 0 5zM5 23L5 18 0 18 0 23z" transform="translate(-981 -190) translate(720) translate(261 190)"/>
                </g>
              </g>
            </g>
          </g>
        </svg>
      </div>
      <flip-item :lineColor="lineColor" :styles="style" :total="9" :current="+timeList[2]"/>
      <flip-item :lineColor="lineColor" :styles="style" :total="9" :current="+timeList[3]"/>
      <div class="colon">
        <svg xmlns="http://www.w3.org/2000/svg" width="5" height="23" viewBox="0 0 5 23">
          <g fill="none" fill-rule="evenodd">
            <g :fill="colonColor" fill-rule="nonzero">
              <g>
                <g>
                  <path d="M5 5L5 0 0 0 0 5zM5 23L5 18 0 18 0 23z" transform="translate(-981 -190) translate(720) translate(261 190)"/>
                </g>
              </g>
            </g>
          </g>
        </svg>
      </div>
      <flip-item :lineColor="lineColor" :styles="style" :total="9" :current="+timeList[4]"/>
      <flip-item :lineColor="lineColor" :styles="style" :total="9" :current="+timeList[5]"/>
      <flip-item :lineColor="lineColor" v-if="mold==='clock'" :styles="style" :total="1" :value="timeList[6]" style="margin-left: 8px"/>
      <flip-item :lineColor="lineColor" v-if="mold==='clock'" :styles="style" :total="1" :value="timeList[7]"/>
    </div>
  </div>
</template>

<script>
import FlipItem from '../plugin/FlipItem'
import { transCountTime, transClockTime } from '../../assets/js/utils'
import mixins from './mixins'

export default {
  name: 'Flip',
  components: {FlipItem},
  mixins: [mixins],
  computed: {
    style () {
      return {
        color: this.color,
        background: this.hasBackground ? this.background : void 0
      }
    },
    colonColor () {
      if (this.color.toUpperCase() === '#FFFFFF') {
        return this.background
      } else {
        return this.color
      }
    },
    lineColor () {
      return this.color.toUpperCase() === '#FFFFFF' ? 'rgba(255,255,255,.2)' : 'rgba(0,0,0,.1)'
    }
  }
}

</script>

<style scoped lang="scss">
.time-box {
  font-family: Bebas Neue;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 17px;
  border-radius: 4px;
}

.colon {
  height: 36px;
  padding: 0 5px;
  display: flex;
  justify-content: space-around;
  flex-direction: column;
  --af-background: 'white';

  &::after,
  &::before {
    content: '';
    display: block;
    width: 5px;
    height: 5px;
    background: var(--af-background);
  }
}
</style>
