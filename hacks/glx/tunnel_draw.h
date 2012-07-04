#define MAX_TEXTURE 10

struct tunnel_state;

extern struct tunnel_state * atunnel_InitTunnel(void);
extern void atunnel_DrawTunnel(struct tunnel_state *,
                       int do_texture, int do_light, GLuint *textures);
extern void atunnel_SplashScreen(struct tunnel_state *,
                         int do_wire, int do_texture, int do_light);
extern void atunnel_FreeTunnel(struct tunnel_state *);

