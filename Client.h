#include "Typedefs.h"
#include "Platform.h"

class IVEngineClient
{
public:
	virtual int					GetIntersectingSurfaces(
		const void* *model,
		const Vector &vCenter,
		const float radius,
		const bool bOnlyVisibleSurfaces,
		void* *pInfos,
		const int nMaxInfos) = 0;

	virtual Vector				GetLightForPoint(const Vector &pos, bool bClamp) = 0;

	virtual void*			*TraceLineMaterialAndLighting(const Vector &start, const Vector &end,
		Vector &diffuseLightColor, Vector& baseColor) = 0;

	virtual const char			*ParseFile(const char *data, char *token, int maxlen) = 0;
	virtual bool				CopyFile(const char *source, const char *destination) = 0;

	virtual void				GetScreenSize(int& width, int& height) = 0;

	virtual void				ServerCmd(const char *szCmdString, bool bReliable = true) = 0;

	virtual void				ClientCmd(const char *szCmdString) = 0;
}