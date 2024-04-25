namespace Piccolo
{
    class RenderDebugConfig
    {
    public:
        struct Animation
        {
            bool show_skeleton = false;
            bool show_bone_name = false;
        };
        struct Camera
        {
            bool show_runtime_info = false;
        };
        struct GameObject
        {
            bool show_bounding_box = false;
        };
        struct MazeShowWay
        {
            bool show_way = false;
        };

        Animation animation;
        Camera camera;
        GameObject gameObject;
        MazeShowWay mazeShowWay;

    };
}