#define MAX_TEXTURE 10

struct tunnel_state;

extern struct tunnel_state * InitTunnel(void);
extern void DrawTunnel(struct tunnel_state *,
                       int do_texture, int do_light, GLuint *textures);
extern void SplashScreen(struct tunnel_state *,
                         int do_wire, int do_texture, int do_light);
extern void FreeTunnel(struct tunnel_state *);

