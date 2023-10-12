<template>
  <div class="viewer-item">
    <div class="time-box" :style="style" :class="{'clock':mold==='clock'}">
      <div class="digit-box">
        <div class="digit">{{ timeList[0] }}</div>
        <div class="digit">{{ timeList[1] }}</div>
        <div class="spot" :class="spotAniClass">
          <div class="spot-dot">·</div>
          <div class="spot-dot">·</div>
        </div>
        <div class="digit">{{ timeList[2] }}</div>
        <div class="digit">{{ timeList[3] }}</div>
        <div class="spot" :class="spotAniClass">
          <div class="spot-dot">·</div>
          <div class="spot-dot">·</div>
        </div>
        <div class="digit">{{ timeList[4] }}</div>
        <div class="digit">{{ timeList[5] }}</div>
      </div>
      <div v-if="mold==='clock'" class="zone">
        <div v-if="timeList[6]==='a'" class="am">
          <svg xmlns="http://www.w3.org/2000/svg" width="48" height="25" viewBox="0 0 48 25">
            <g fill="none" fill-rule="evenodd">
              <g :fill="style.color||'#fff'" fill-rule="nonzero">
                <g>
                  <path
                      d="M8.998 18.17c1.424 0 2.688-.301 3.794-.905 1.105-.604 1.999-1.447 2.683-2.53h.05V18h1.761V.996H15.44v3.332h-.068c-.706-1.105-1.623-1.965-2.751-2.58-1.128-.616-2.359-.923-3.692-.923-1.583 0-3.008.39-4.272 1.17-1.265.78-2.245 1.84-2.94 3.18C1.022 6.512.675 8.001.675 9.642c0 1.265.22 2.433.658 3.503.438 1.071 1.034 1.972 1.786 2.7.752.73 1.635 1.3 2.649 1.71 1.014.41 2.09.615 3.23.615zm.12-1.742c-1.277 0-2.416-.296-3.419-.889-1.002-.592-1.777-1.404-2.324-2.435-.547-1.031-.82-2.18-.82-3.444 0-1.048.173-2.02.52-2.914.348-.894.821-1.643 1.42-2.247.598-.604 1.295-1.074 2.093-1.41.797-.336 1.646-.504 2.546-.504 1.22 0 2.316.316 3.29.948.974.633 1.732 1.487 2.273 2.564.541 1.077.812 2.264.812 3.563 0 .991-.174 1.911-.521 2.76-.348.849-.812 1.564-1.393 2.145s-1.26 1.037-2.034 1.367c-.775.33-1.59.496-2.444.496zm27.275 1.606v-8.46c0-1.047.145-2.013.435-2.896.29-.883.687-1.62 1.188-2.213.501-.593 1.088-1.057 1.76-1.393.673-.336 1.39-.504 2.154-.504.717 0 1.338.105 1.862.316.525.21.975.553 1.35 1.025.377.473.659 1.103.847 1.889.188.786.282 1.737.282 2.854v9.382h1.845V8.071c0-2.404-.484-4.213-1.452-5.426-.969-1.214-2.404-1.82-4.307-1.82-.889 0-1.732.145-2.53.435-.797.291-1.52.713-2.17 1.265-.65.553-1.196 1.216-1.64 1.991h-.103c-.41-1.219-1.057-2.139-1.94-2.76-.883-.62-1.974-.931-3.272-.931-1.185 0-2.31.299-3.376.897-1.065.598-1.945 1.433-2.64 2.504h-.068V.996h-1.846V18h1.846V9.575c0-2.097.538-3.789 1.615-5.076 1.076-1.287 2.424-1.931 4.041-1.931 1.368 0 2.421.464 3.162 1.393.74.928 1.11 2.44 1.11 4.537v9.536h1.847z"
                      transform="translate(-1081 -1493) translate(1081 1493)"/>
                </g>
              </g>
            </g>
          </svg>
        </div>
        <div v-else-if="timeList[6]==='p'" class="pm">
          <svg xmlns="http://www.w3.org/2000/svg" width="48" height="25" viewBox="0 0 48 25">
            <g fill="none" fill-rule="evenodd">
              <g :fill="style.color||'#fff'" fill-rule="nonzero">
                <g>
                  <path
                      d="M2.683 24.921V14.855h.068c.684 1.049 1.57 1.863 2.657 2.444 1.089.581 2.333.872 3.735.872 1.139 0 2.216-.205 3.23-.615 1.014-.41 1.897-.98 2.648-1.71.752-.728 1.348-1.629 1.786-2.7.439-1.07.658-2.238.658-3.503 0-1.64-.347-3.13-1.042-4.469-.695-1.339-1.675-2.398-2.94-3.179-1.264-.78-2.688-1.17-4.272-1.17-1.333 0-2.566.31-3.7.931-1.134.621-2.054 1.49-2.76 2.606h-.068V.996H.837V24.92h1.846zm6.34-8.493c-.684 0-1.344-.103-1.982-.308-.638-.205-1.222-.507-1.752-.906-.53-.398-.994-.871-1.393-1.418-.399-.547-.71-1.174-.931-1.88-.223-.706-.334-1.458-.334-2.256 0-1.299.27-2.486.812-3.563.541-1.077 1.299-1.931 2.273-2.564.974-.632 2.07-.948 3.29-.948.889 0 1.734.168 2.538.504.803.336 1.504.806 2.102 1.41.598.604 1.07 1.353 1.418 2.247.348.895.521 1.866.521 2.914 0 1.265-.273 2.413-.82 3.444-.547 1.03-1.321 1.843-2.324 2.435-1.003.593-2.142.889-3.418.889zm26.643 1.606v-8.46c0-1.047.145-2.013.436-2.896.29-.883.686-1.62 1.188-2.213.5-.593 1.088-1.057 1.76-1.393.672-.336 1.39-.504 2.153-.504.718 0 1.339.105 1.863.316.524.21.974.553 1.35 1.025.376.473.658 1.103.846 1.889.188.786.282 1.737.282 2.854v9.382h1.846V8.071c0-2.404-.485-4.213-1.453-5.426-.968-1.214-2.404-1.82-4.307-1.82-.888 0-1.731.145-2.529.435-.797.291-1.52.713-2.17 1.265-.65.553-1.197 1.216-1.641 1.991h-.102c-.41-1.219-1.057-2.139-1.94-2.76-.883-.62-1.974-.931-3.273-.931-1.185 0-2.31.299-3.375.897-1.065.598-1.946 1.433-2.64 2.504h-.069V.996h-1.846V18h1.846V9.575c0-2.097.538-3.789 1.615-5.076 1.077-1.287 2.424-1.931 4.042-1.931 1.367 0 2.42.464 3.161 1.393.741.928 1.111 2.44 1.111 4.537v9.536h1.846z"
                      transform="translate(-1145 -1493) translate(1145 1493)"/>
                </g>
              </g>
            </g>
          </svg>

        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { transCountTime, transClockTime } from '../../assets/js/utils'
import mixins from './mixins'

export default {
  name: 'Round',
  mixins: [mixins],
  computed: {
    style () {
      return {
        background: this.background,
        color: this.color
      }
    }
  },
  methods: {
    spotAni () {
      this.spotAniClass = 'spot-out'
      setTimeout(() => {
        this.spotAniClass = 'spot-in'
      }, 100)
    }
  }
}
</script>

<style scoped lang="scss">
.digit-box > div {
  margin-bottom: 2px;
}

.time-box {
  font-family: Major Mono Display;
  display: flex;
  border-radius: 54px;
  font-size: 52px;
  padding: 14px 27px 16px 27px;
  object-fit: contain;
  box-shadow: 0 0 6px 0 rgba(0, 0, 0, 0.2);
  height: 51px;
}

.time-box.clock {
  padding: 14px 29px 16px 27px;
}

.digit-box {
  display: flex;
  align-items: center;
  justify-content: center;
}

.digit {
  width: 37px;
}

.zone {
  font-size: 24px;
  margin-left: 5px;
  font-weight: bold;
  margin-bottom: 6px;
  position: relative;
  top: 27px;
  width: 49px;
}

.spot {
  font-size: 24px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  opacity: .8;
  width: 12px;

  .spot-dot {
    height: 14px;
    line-height: 19px;
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
    opacity: .8;
  }
}

@keyframes SpotOut {
  from {
    opacity: .8;
  }
  to {
    opacity: 0;
  }
}
</style>
