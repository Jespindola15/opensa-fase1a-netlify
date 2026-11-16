import{h as e}from"./build-texture-DpuRTxHG.js";var t={value:0},n={value:new e(.55,.62,.8)},r={value:new e(.18,.2,.26)},i={value:.1},a=`#include <emissivemap_fragment>
	{
		float nfHemi = normal.y * 0.5 + 0.5; // view-space up (camera-relative — prototype simplification)
		vec3 nfMoon = mix( uFillGround, uFillSky, nfHemi ) * uNightFill;
		totalEmissiveRadiance += nfMoon * diffuseColor.rgb;
		float nfRim = pow( clamp( 1.0 - dot( normal, normalize( vViewPosition ) ), 0.0, 1.0 ), 3.0 );
		totalEmissiveRadiance += uFillRim * nfRim * uNightFill * uFillSky;
	}`;function o(e){let o=e.onBeforeCompile.bind(e),s=e.customProgramCacheKey.bind(e);e.customProgramCacheKey=()=>`${s()}|nightFill`,e.onBeforeCompile=(e,s)=>{o(e,s),e.uniforms.uNightFill=t,e.uniforms.uFillSky=n,e.uniforms.uFillGround=r,e.uniforms.uFillRim=i,e.fragmentShader=`uniform float uNightFill;
uniform vec3 uFillSky;
uniform vec3 uFillGround;
uniform float uFillRim;
`+e.fragmentShader.replace(`#include <emissivemap_fragment>`,a)},e.needsUpdate=!0}export{i as n,t as r,o as t};