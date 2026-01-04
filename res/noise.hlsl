float rand(float n){return frac(sin(n) * 43758.5453123);}

float noise(float p){
	float fl = floor(p);
  float fc = frac(p);
	return lerp(rand(fl), rand(fl + 1.0), fc);
}
