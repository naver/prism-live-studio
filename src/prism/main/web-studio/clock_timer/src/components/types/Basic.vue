<template>
  <div class="viewer-item">
    <div class="time-box" :style="style">
      <div class="digit">{{ timeList[0] }}</div>
      <div class="digit" style="margin-left: 0">{{ timeList[1] }}</div>
      <div class="spot" style="margin-left: 13px" :class="spotAniClass">
        <div class="spot-dot">·</div>
        <div class="spot-dot">·</div>
      </div>
      <div class="digit" style="margin-left: 15px">{{ timeList[2] }}</div>
      <div class="digit" style="margin-left: -1px">{{ timeList[3] }}</div>
      <div class="spot" style="margin-left: 14px" :class="spotAniClass">
        <div class="spot-dot">·</div>
        <div class="spot-dot">·</div>
      </div>
      <div class="digit" style="margin-left: 13px;">{{ timeList[4] }}</div>
      <div class="digit" style="margin-left: 1px">{{ timeList[5] }}</div>
      <div v-if="mold==='clock'" class="zone" style="margin-left: 13px">
        {{ `${timeList[6] || ''}${timeList[7] || ''}` }}
      </div>
    </div>
  </div>
</template>

<script>
import { transCountTime, transClockTime } from '../../assets/js/utils'
import { CountDown, Clock } from '../../assets/js/CountClock'
import mixins from './mixins'

export default {
  name: 'Basic',
  mixins: [mixins]
}
</script>

<style scoped lang="scss">
.time-box > div {
  margin-bottom: 3px;
}

.time-box {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 12px 0 12px;
  border-radius: 3px;
  height: 67px;
}

.margin-right-12 {
  margin-right: 12px;
}

.zone {
  position: relative;
  bottom: 0;
}

.spot {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;

  .spot-dot {
    height: 18px;
    line-height: 24px;
  }
}

.spot-in {
  animation: SpotIn 0.1s linear both;
}

.spot-out {
  animation: SpotOut 0.1s linear both;
}

@keyframes SpotIn {
  from {
    opacity: 0;
  }
  to {
    opacity: 1;
  }
}

@keyframes SpotOut {
  from {
    opacity: 1;
  }
  to {
    opacity: 0;
  }
}
</style>
