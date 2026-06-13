<template>
  <svg
    :width="size"
    :height="size"
    viewBox="0 0 200 200"
    xmlns="http://www.w3.org/2000/svg"
    class="flower-fairy"
    :class="moodClass"
  >
    <!-- Glow aura -->
    <circle cx="100" cy="95" r="70" :fill="auraColor" opacity="0.12" class="aura" />

    <!-- Sparkle particles -->
    <g class="sparkles">
      <circle cx="45" cy="55" r="2" :fill="sparkleColor" class="sp sp1" />
      <circle cx="155" cy="48" r="1.5" :fill="sparkleColor" class="sp sp2" />
      <circle cx="38" cy="110" r="1.8" :fill="sparkleColor" class="sp sp3" />
      <circle cx="162" cy="105" r="2.2" :fill="sparkleColor" class="sp sp4" />
      <circle cx="70" cy="30" r="1.2" :fill="sparkleColor" class="sp sp5" />
      <circle cx="130" cy="35" r="1.6" :fill="sparkleColor" class="sp sp6" />
      <circle cx="55" cy="140" r="1.4" :fill="sparkleColor" class="sp sp7" />
      <circle cx="145" cy="135" r="1.8" :fill="sparkleColor" class="sp sp8" />
    </g>

    <!-- Left Wing -->
    <g class="wing-left" :opacity="wingOpacity">
      <path d="M62,78 Q30,50 25,70 Q20,90 45,95 Q55,93 62,88" :fill="wingColorL" stroke="none"/>
      <path d="M62,88 Q35,75 28,85 Q22,100 42,105 Q52,103 62,100" :fill="wingColorL2" stroke="none"/>
      <!-- Wing veins -->
      <path d="M62,82 Q40,60 30,70" stroke="rgba(255,255,255,0.3)" stroke-width="0.5" fill="none"/>
      <path d="M62,92 Q40,80 32,88" stroke="rgba(255,255,255,0.2)" stroke-width="0.5" fill="none"/>
    </g>

    <!-- Right Wing -->
    <g class="wing-right" :opacity="wingOpacity">
      <path d="M138,78 Q170,50 175,70 Q180,90 155,95 Q145,93 138,88" :fill="wingColorR" stroke="none"/>
      <path d="M138,88 Q165,75 172,85 Q178,100 158,105 Q148,103 138,100" :fill="wingColorR2" stroke="none"/>
      <path d="M138,82 Q160,60 170,70" stroke="rgba(255,255,255,0.3)" stroke-width="0.5" fill="none"/>
      <path d="M138,92 Q160,80 168,88" stroke="rgba(255,255,255,0.2)" stroke-width="0.5" fill="none"/>
    </g>

    <!-- Body / Dress -->
    <g class="body">
      <!-- Dress skirt (petal-shaped) -->
      <path d="M80,115 Q78,135 72,148 Q75,150 85,145 Q92,148 100,150 Q108,148 115,145 Q125,150 128,148 Q122,135 120,115 Z"
            :fill="dressColor" stroke="none"/>
      <!-- Dress bodice -->
      <path d="M85,90 Q82,105 80,115 L120,115 Q118,105 115,90 Z"
            :fill="bodiceColor" stroke="none"/>
      <!-- Dress neckline detail -->
      <path d="M85,90 Q100,95 115,90" stroke="rgba(255,255,255,0.4)" stroke-width="1" fill="none"/>
    </g>

    <!-- Arms -->
    <g class="arms" :transform="armTransform">
      <!-- Left arm -->
      <path d="M82,95 Q70,100 65,108" stroke="skinColor" stroke-width="3" stroke-linecap="round" fill="none"/>
      <!-- Right arm -->
      <path d="M118,95 Q130,100 135,108" stroke="skinColor" stroke-width="3" stroke-linecap="round" fill="none"/>
      <!-- Hands (tiny circles) -->
      <circle cx="65" cy="108" r="3" :fill="skinColor"/>
      <circle cx="135" cy="108" r="3" :fill="skinColor"/>
    </g>

    <!-- Neck -->
    <path d="M92,82 L92,88 M108,82 L108,88" stroke="skinColor" stroke-width="2.5" stroke-linecap="round" fill="none"/>

    <!-- Head -->
    <circle cx="100" cy="72" r="20" :fill="skinColor" stroke="none"/>

    <!-- Face -->
    <g class="face">
      <!-- Eyes -->
      <g v-if="moisture >= 25 || moisture == null">
        <!-- Happy/content eyes -->
        <ellipse cx="92" cy="70" rx="3.5" ry="4" fill="#2d3436"/>
        <ellipse cx="108" cy="70" rx="3.5" ry="4" fill="#2d3436"/>
        <!-- Eye highlights -->
        <circle cx="93.5" cy="68.5" r="1.2" fill="white" opacity="0.8"/>
        <circle cx="109.5" cy="68.5" r="1.2" fill="white" opacity="0.8"/>
        <!-- Eyebrows -->
        <path d="M87,65 Q92,62 96,65" stroke="#2d3436" stroke-width="1" fill="none" :opacity="eyebrowOpacity"/>
        <path d="M104,65 Q108,62 113,65" stroke="#2d3436" stroke-width="1" fill="none" :opacity="eyebrowOpacity"/>
      </g>
      <!-- Sad/worried eyes when dry -->
      <g v-else>
        <!-- Worried squint eyes -->
        <path d="M88,70 Q92,67 96,70" stroke="#2d3436" stroke-width="2" fill="none"/>
        <path d="M104,70 Q108,67 112,70" stroke="#2d3436" stroke-width="2" fill="none"/>
        <!-- Worried eyebrows -->
        <path d="M87,63 Q92,66 96,64" stroke="#2d3436" stroke-width="1.2" fill="none"/>
        <path d="M104,64 Q108,66 113,63" stroke="#2d3436" stroke-width="1.2" fill="none"/>
      </g>

      <!-- Mouth -->
      <path v-if="moisture >= 60" d="M93,78 Q100,84 107,78" stroke="#e17055" stroke-width="1.5" fill="none" stroke-linecap="round"/>
      <path v-else-if="moisture >= 40" d="M94,78 Q100,81 106,78" stroke="#e17055" stroke-width="1.3" fill="none" stroke-linecap="round"/>
      <path v-else-if="moisture >= 25" d="M94,80 Q100,78 106,80" stroke="#e17055" stroke-width="1.2" fill="none" stroke-linecap="round"/>
      <path v-else-if="moisture != null" d="M94,80 L106,80" stroke="#e17055" stroke-width="1.5" fill="none" stroke-linecap="round"/>
      <path v-else d="M95,80 Q100,82 105,80" stroke="#b2bec3" stroke-width="1" fill="none" stroke-linecap="round"/>

      <!-- Blush (when happy) -->
      <circle v-if="moisture >= 50" cx="86" cy="76" r="4" fill="rgba(255,118,117,0.25)"/>
      <circle v-if="moisture >= 50" cx="114" cy="76" r="4" fill="rgba(255,118,117,0.25)"/>

      <!-- Tear (when very dry) -->
      <g v-if="moisture != null && moisture < 25">
        <ellipse cx="88" cy="73" rx="2" ry="3" fill="#74b9ff" opacity="0.7" class="tear"/>
        <ellipse cx="112" cy="73" rx="2" ry="3" fill="#74b9ff" opacity="0.7" class="tear tear2"/>
      </g>
    </g>

    <!-- Hair -->
    <g class="hair">
      <path d="M80,72 Q78,55 85,50 Q95,44 100,42 Q105,44 115,50 Q122,55 120,72"
            :fill="hairColor" stroke="none"/>
      <!-- Side hair strands -->
      <path d="M80,72 Q76,82 74,90 Q73,92 75,88" :fill="hairColor" stroke="none"/>
      <path d="M120,72 Q124,82 126,90 Q127,92 125,88" :fill="hairColor" stroke="none"/>
    </g>

    <!-- Flower Crown -->
    <g class="flower-crown">
      <!-- Crown base -->
      <path d="M80,55 Q100,48 120,55" stroke="none" :fill="crownBaseColor"/>
      <!-- Flowers on crown -->
      <g v-for="(f, i) in crownFlowers" :key="i" :transform="`translate(${f.x},${f.y})`">
        <circle r="4" :fill="f.color"/>
        <circle r="1.5" fill="#ffeaa7" opacity="0.8"/>
      </g>
      <!-- Leaf accents -->
      <path d="M82,56 Q78,50 80,52" :fill="leafColor" stroke="none"/>
      <path d="M118,56 Q122,50 120,52" :fill="leafColor" stroke="none"/>
    </g>

    <!-- Feet -->
    <circle cx="85" cy="150" r="4" :fill="skinColor"/>
    <circle cx="115" cy="150" r="4" :fill="skinColor"/>

    <!-- Ground flower decoration -->
    <g class="ground-flower" v-if="moisture >= 40">
      <path d="M85,155 Q88,158 85,160 Q82,158 85,155" :fill="flowerAccentColor"/>
      <path d="M115,155 Q118,158 115,160 Q112,158 115,155" :fill="flowerAccentColor"/>
    </g>

    <!-- Water droplet (when very dry, fairy holds water) -->
    <g v-if="moisture != null && moisture < 30" class="water-drop">
      <path d="M135,108 Q137,102 139,108 Q137,112 135,108" fill="#74b9ff" opacity="0.7"/>
    </g>
  </svg>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  moisture: { type: Number, default: null },
  size: { type: Number, default: 80 }
})

// Mood classification
const moodClass = computed(() => {
  const m = props.moisture
  if (m == null) return 'mood-sleeping'
  if (m >= 60) return 'mood-happy'
  if (m >= 40) return 'mood-content'
  if (m >= 25) return 'mood-worried'
  return 'mood-sad'
})

// Dynamic colors based on moisture
const skinColor = computed(() => '#ffeaa7')
const hairColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#b2bec3'
  if (m >= 60) return '#e17055'
  if (m >= 40) return '#d35400'
  return '#a0522d'
})

const wingColorL = computed(() => {
  const m = props.moisture
  if (m == null) return 'rgba(178,190,195,0.3)'
  if (m >= 60) return 'rgba(129,199,132,0.45)'
  if (m >= 40) return 'rgba(255,183,77,0.4)'
  if (m >= 25) return 'rgba(255,152,0,0.35)'
  return 'rgba(239,83,80,0.3)'
})

const wingColorL2 = computed(() => {
  const m = props.moisture
  if (m == null) return 'rgba(178,190,195,0.2)'
  if (m >= 60) return 'rgba(165,214,167,0.35)'
  if (m >= 40) return 'rgba(255,204,128,0.3)'
  if (m >= 25) return 'rgba(255,167,38,0.25)'
  return 'rgba(255,138,128,0.2)'
})

const wingColorR = computed(() => wingColorL.value)
const wingColorR2 = computed(() => wingColorL2.value)

const wingOpacity = computed(() => {
  const m = props.moisture
  if (m == null) return 0.4
  if (m >= 60) return 1
  if (m >= 40) return 0.85
  return 0.7
})

const dressColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#dfe6e9'
  if (m >= 60) return '#a5d6a7'
  if (m >= 40) return '#ffe0b2'
  if (m >= 25) return '#ffcc80'
  return '#ffcdd2'
})

const bodiceColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#b2bec3'
  if (m >= 60) return '#81c784'
  if (m >= 40) return '#ffb74d'
  if (m >= 25) return '#ffa726'
  return '#ef5350'
})

const auraColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#b2bec3'
  if (m >= 60) return '#81c784'
  if (m >= 40) return '#ffb74d'
  if (m >= 25) return '#ffa726'
  return '#ef5350'
})

const sparkleColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#dfe6e9'
  if (m >= 60) return '#ffeaa7'
  if (m >= 40) return '#fff3e0'
  return '#ffeb3b'
})

const crownBaseColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#b2bec3'
  return '#81c784'
})

const leafColor = computed(() => {
  const m = props.moisture
  if (m == null) return '#b2bec3'
  if (m >= 40) return '#66bb6a'
  if (m >= 25) return '#ffa726'
  return '#a1887f'
})

const flowerAccentColor = computed(() => {
  const m = props.moisture
  if (m >= 60) return '#f06292'
  if (m >= 40) return '#ff8a65'
  return '#ffab91'
})

const eyebrowOpacity = computed(() => {
  const m = props.moisture
  if (m == null) return 0.3
  if (m >= 60) return 0.7
  if (m >= 40) return 0.5
  return 0.8
})

const armTransform = computed(() => {
  const m = props.moisture
  if (m != null && m < 25) return 'rotate(-5, 100, 100)' // droopy arms
  return ''
})

// Crown flowers array
const crownFlowers = computed(() => {
  const m = props.moisture
  let colors = ['#f06292', '#ffeaa7', '#81c784']
  if (m != null && m < 25) colors = ['#a1887f', '#b2bec3', '#a1887f']
  if (m == null) colors = ['#dfe6e9', '#b2bec3', '#dfe6e9']
  return [
    { x: 82, y: 52, color: colors[0] },
    { x: 100, y: 46, color: colors[1] },
    { x: 118, y: 52, color: colors[2] }
  ]
})
</script>

<style scoped>
.flower-fairy {
  display: inline-block;
  transition: all 0.5s ease;
}

/* Floating animation */
.mood-happy {
  animation: floatHappy 3s ease-in-out infinite;
}
.mood-content {
  animation: floatGentle 4s ease-in-out infinite;
}
.mood-worried {
  animation: floatSlow 5s ease-in-out infinite;
}
.mood-sad {
  animation: floatSlow 5s ease-in-out infinite;
  filter: brightness(0.85);
}
.mood-sleeping {
  animation: floatDormant 6s ease-in-out infinite;
  filter: brightness(0.6);
}

@keyframes floatHappy {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-6px); }
}
@keyframes floatGentle {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-4px); }
}
@keyframes floatSlow {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-2px); }
}
@keyframes floatDormant {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-1px); }
}

/* Sparkle animation */
.sp {
  animation: sparkle 2.5s ease-in-out infinite;
}
.sp1 { animation-delay: 0s; }
.sp2 { animation-delay: 0.3s; }
.sp3 { animation-delay: 0.6s; }
.sp4 { animation-delay: 0.9s; }
.sp5 { animation-delay: 1.2s; }
.sp6 { animation-delay: 1.5s; }
.sp7 { animation-delay: 1.8s; }
.sp8 { animation-delay: 2.1s; }

@keyframes sparkle {
  0%, 100% { opacity: 0; r: 0; }
  50% { opacity: 0.8; }
}

/* Wing flutter */
.mood-happy .wing-left,
.mood-happy .wing-right {
  animation: flutterFast 0.6s ease-in-out infinite;
}
.mood-content .wing-left,
.mood-content .wing-right {
  animation: flutterGentle 1.2s ease-in-out infinite;
}
.mood-worried .wing-left,
.mood-worried .wing-right {
  animation: flutterSlow 2s ease-in-out infinite;
}
.mood-sad .wing-left,
.mood-sad .wing-right {
  animation: flutterSlow 2.5s ease-in-out infinite;
}
.mood-sleeping .wing-left,
.mood-sleeping .wing-right {
  animation: none;
}

@keyframes flutterFast {
  0%, 100% { transform: scaleX(1); }
  50% { transform: scaleX(0.85); }
}
@keyframes flutterGentle {
  0%, 100% { transform: scaleX(1); }
  50% { transform: scaleX(0.9); }
}
@keyframes flutterSlow {
  0%, 100% { transform: scaleX(1); }
  50% { transform: scaleX(0.93); }
}

/* Tear animation */
.tear {
  animation: tearDrop 1.5s ease-in infinite;
}
.tear2 {
  animation: tearDrop 1.5s ease-in infinite 0.5s;
}

@keyframes tearDrop {
  0% { transform: translateY(0); opacity: 0.7; }
  80% { transform: translateY(5px); opacity: 0.5; }
  100% { transform: translateY(6px); opacity: 0; }
}

/* Water drop animation */
.water-drop {
  animation: dropBounce 2s ease-in-out infinite;
}
@keyframes dropBounce {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-3px); }
}

/* Aura pulse */
.aura {
  animation: auraPulse 3s ease-in-out infinite;
}
@keyframes auraPulse {
  0%, 100% { opacity: 0.08; }
  50% { opacity: 0.18; }
}
</style>
