#ifndef OUI_H_
#define OUI_H_

int oui_load(const char *path);

const char *oui_lookup(const char *mac);

void oui_free(void);


#endif // OUI_H_
