<template>
  <div :class="{play: isPlay}">
    <ul class="flip" v-if="value">
      <li class="item">
        <div class="up" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
        <div class="down" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
      </li>
      <li class="item">
        <div class="up" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
        <div class="down" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
      </li>
      <li class="item">
        <div class="up" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
        <div class="down" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
      </li>
      <li class="item">
        <div class="up" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
        <div class="down" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
      </li>
      <li class="item">
        <div class="up" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
        <div class="down" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ value }}</div>
        </div>
      </li>
    </ul>
    <ul v-else class="flip">
      <li
          class="item"
          v-for="(item, key) in total + 1"
          :class="{active: current === key, before: key === before}"
          :key="item"
      >
        <div class="up" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ key }}</div>
        </div>
        <div class="down" :style="{'--line-color':lineColor}">
          <div class="shadow"></div>
          <div class="inn" :class="borderFixClass" :style="styles">{{ key }}</div>
        </div>
      </li>
    </ul>
  </div>
</template>

<script>
export default {
  props: {
    total: {
      type: Number,
      default: 9
    },
    current: {
      type: Number,
      default: -1
    },
    value: {
      type: String,
      default: ''
    },
    styles: {
      type: Object,
      default () {
        return {
          color: '#ff4d4d',
          background: 'white'
        }
      }
    },
    lineColor: {
      type: String,
      default: 'rgba(0, 0, 0, .2)'
    }
  },
  computed: {
    borderFixClass () {
      return Number.isNaN(+this.value) ? 'border-3' : ''
    }
  },
  data () {
    return {
      before: this.total === this.current ? -1 : this.total,
      isPlay: false
    }
  },
  watch: {
    current (current, preCurrent) {
      this.before = preCurrent
      if (!this.isPlay) {
        this.isPlay = true
      }
    }
  }
}
</script>

<style lang="scss" scoped>
$width: 37px;
$height: 61px;
$fontSize: 50px;
$lineWidth: 1px;
$radius: 4px;
$cubic: linear;
$animate-duration: 0.2s;

.border-3 {
  border-radius: 4px !important;
}

.flip {
  position: relative;
  margin: 5px 3px;
  width: $width;
  height: $height;
  font-size: $fontSize;
  border-radius: $radius;
  box-shadow: 0 2px 4px 0 rgba(0, 0, 0, 0.2);;

  .item {
    list-style: none;
    z-index: 1;
    position: absolute;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    perspective: 200px;
    transition: opacity 0.3s;

    &.active {
      z-index: 2;
    }

    &:first-child {
      z-index: 2;
    }

    .up,
    .down {
      z-index: 1;
      position: absolute;
      left: 0;
      width: 100%;
      height: 50%;
      overflow: hidden;
    }

    .up {
      transform-origin: 50% 100%;
      top: 0;
      --line-color: rgba(0, 0, 0, .2);

      &:after {
        content: '';
        position: absolute;
        top: ($height - $lineWidth) / 2;
        left: 0;
        z-index: 5;
        width: 100%;
        height: $lineWidth;
        background-color: var(--line-color);
      }
    }

    .down {
      transform-origin: 50% 0;
      bottom: 0;
      transition: opacity 0.3s;
      --line-color: rgba(0, 0, 0, .2);

      &:after {
        content: '';
        position: absolute;
        bottom: ($height - $lineWidth) / 2;
        left: 0;
        z-index: 5;
        width: 100%;
        height: $lineWidth;
        background-color: var(--line-color);
      }
    }

    .inn {
      position: absolute;
      left: 0;
      z-index: 1;
      width: 100%;
      height: 200%;
      text-align: center;
      background-color: white;
      border-radius: $radius;
    }

    .up .inn {
      top: 0;
    }

    .down .inn {
      bottom: 0;
    }
  }
}

.play {
  .item {
    &.before {
      z-index: 3;
    }

    &.active {
      animation: asd 0.2s 0.2s linear both;
      z-index: 2;
    }

    &.before .up {
      z-index: 2;
      animation: turn-up 0.2s cubic-bezier(.67, 0, 1, 1) both;
    }

    &.active .down {
      z-index: 2;
      animation: turn-down 0.2s 0.2s cubic-bezier(0, 0, .37, 1) both;
    }
  }
}

@keyframes turn-down {
  0% {
    transform: rotateX(90deg);
  }
  100% {
    transform: rotateX(0deg);
  }
}

@keyframes turn-up {
  0% {
    transform: rotateX(0deg);
  }
  100% {
    transform: rotateX(-90deg);
  }
}

@keyframes asd {
  0% {
    z-index: 2;
  }
  5% {
    z-index: 4;
  }
  100% {
    z-index: 4;
  }
}

.play {
  .shadow {
    position: absolute;
    width: 100%;
    height: 100%;
    z-index: 2;
  }

  .before .up .shadow {
    background: linear-gradient(rgba(0, 0, 0, 0.1) 0%, rgba(0, 0, 0, .5) 100%);
    animation: show $animate-duration linear both;
  }

  .active .up .shadow {
    background: linear-gradient(rgba(0, 0, 0, 0.1) 0%, rgba(0, 0, 0, .5) 100%);
    animation: hide $animate-duration linear both;
    border-top-left-radius: $radius;
    border-top-right-radius: $radius;
  }

  .before .down .shadow {
    background: linear-gradient(rgba(0, 0, 0, .5) 0%, rgba(0, 0, 0, 0.1) 100%);
    animation: show $animate-duration linear both;
    border-bottom-left-radius: $radius;
    border-bottom-right-radius: $radius;
  }

  .active .down .shadow {
    background: linear-gradient(rgba(0, 0, 0, .5) 0%, rgba(0, 0, 0, 0.1) 100%);
    animation: hide $animate-duration linear both;
  }
}

@keyframes show {
  0% {
    opacity: 0;
  }
  100% {
    opacity: 1;
  }
}

@keyframes hide {
  0% {
    opacity: 1;
  }
  100% {
    opacity: 0;
  }
}
</style>
