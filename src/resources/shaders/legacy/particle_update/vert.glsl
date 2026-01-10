uniform float dt;
uniform vec2 wind;
uniform vec4 bounding_box;
uniform int particle_type; // 0 for rain, 1 for fog
uniform vec2 random_seed;

in vec3 position;
in vec3 velocity;
in float lifetime;

out vec3 out_position;
out vec3 out_velocity;
out float out_lifetime;

// Basic pseudo-random number generator
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    out_lifetime = lifetime - dt;
    if (out_lifetime <= 0.0) {
        // Respawn particle
        float rand_x = rand(vec2(gl_VertexID) * random_seed.x);
        float rand_y = rand(vec2(gl_VertexID) * random_seed.y);
        out_position = vec3(bounding_box.x + rand_x * bounding_box.z, bounding_box.y + rand_y * bounding_box.w, 0.0);
        if (particle_type == 0) { // Rain
            out_velocity = vec3(rand(vec2(gl_VertexID) * random_seed.x) * 0.2 - 0.1, -1.0, 0.0); // Fall mostly straight down
        } else if (particle_type == 2) { // Snow
            out_velocity = vec3(rand(vec2(gl_VertexID) * random_seed.x * 2.0) - 0.5, rand(vec2(gl_VertexID) * random_seed.y * 2.0) - 0.5, 0.0) * 0.2;
        } else { // Fog
            out_velocity = vec3(rand(vec2(gl_VertexID) * random_seed.x * 2.0) - 0.5, rand(vec2(gl_VertexID) * random_seed.y * 2.0) - 0.5, 0.0) * 0.1;
        }
        out_lifetime = 1.0;
    } else {
        out_position = position + velocity * dt;
        if (particle_type == 0) { // Rain
            out_velocity = velocity + vec3(wind, 0.0) * dt;
        } else { // Fog or Snow
            out_velocity = velocity + vec3(wind, 0.0) * dt * 0.5;
        }
    }
}
