// 弹道结算
// 只考虑水平方向的空气阻力

#include <math.h>
#include <stdio.h>

#include "SolveTrajectory.h"

struct SolveTrajectory st;
struct tar_pos tar_position[4];
float t = 0.0f; // 飞行时间

/*
@brief 初始化
@param pitch:rad
@param yaw:rad
@param v:m/s
@param k:弹道系数
*/
void GimbalControlInit(float pitch, float yaw, float tar_yaw , float r1, float r2, float z2, float v, float k)
{
    st.current_pitch = pitch;
    st.current_yaw = yaw;
    st.current_v = v;
    st._k = k;
    st.tar_yaw = tar_yaw;
    st.tar_r1 = r1;
    st.tar_r2 = r2;
    st.z2 = z2;
    printf("init %f,%f,%f,%f\n", st.current_pitch, st.current_yaw, st.current_v, st._k);
}

/*
@brief 弹道模型
@param x:m 距离
@param v:m/s 速度
@param angle:rad 角度
@return y:m
*/
float GimbalControlBulletModel(float x, float v, float angle)
{
    float y;
    t = (float)((exp(st._k * x) - 1) / (st._k * v * cos(angle)));
    y = (float)(v * sin(angle) * t - GRAVITY * t * t / 2);
    printf("model %f %f\n", t, y);
    return y;
}

/*
@brief pitch轴解算
@param x:m 距离
@param y:m 高度
@param v:m/s
@return angle_pitch:rad
*/
float GimbalControlGetPitch(float x, float y, float v)
{
    float y_temp, y_actual, dy;
    float angle_pitch;
    y_temp = y;
    // iteration
    int i = 0;
    for (i = 0; i < 20; i++)
    {
        angle_pitch = (float)atan2(y_temp, x); // rad
        y_actual = GimbalControlBulletModel(x, v, angle_pitch);
        dy = 0.3*(y - y_actual);
        y_temp = y_temp + dy;
        printf("iteration num %d: angle_pitch %f, temp target y:%f, err of y:%f, x:%f\n", i + 1, angle_pitch * 180 / PI, y_temp, dy,x);
        if (fabsf(dy) < 0.00001)
        {
            break;
        }
    }
    return angle_pitch;
}

/*
@brief 世界坐标系转换到云台坐标系
@param xw:ROS坐标系下的x
@param yw:ROS坐标系下的y
@param zw:ROS坐标系下的z
@param vxw:ROS坐标系下的vx
@param vyw:ROS坐标系下的vy
@param vzw:ROS坐标系下的vz
@param timestamp_start:开始时间戳
@param pitch:rad  传出pitch
@param yaw:rad    传出yaw
*/
void GimbalControlTransform(float xw, float yw, float zw,
                            float vxw, float vyw, float vzw,
                            int timestamp_start, float *pitch, float *yaw,
                            float *aim_x, float *aim_y, float *aim_z)
{
    float x_static = 0.19133; //相机前推的距离
    float z_static = 0.21265; //yaw轴电机到枪口水平面的垂直距离
    int timestamp_now = timestamp_start + 50; // 假设当前时间戳=开始时间戳+200ms
    // TODO：获取当前时间戳
    
    // 线性预测
    // 计算通信及解算时间戳延时+子弹飞行时间  考虑了200ms的通信延时
    
    //计算四块装甲板的位置
    int use_1 = 1;
    for (int i=0; i<4; i++) {
        float tmp_yaw = st.tar_yaw + i * PI/2.0;
        float r = use_1 ? st.tar_r1 : st.tar_r2;
        tar_position[i].x = xw - r*cos(tmp_yaw);
        tar_position[i].y = yw - r*sin(tmp_yaw);
        tar_position[i].z = use_1 ? zw : st.z2;
        use_1 = !use_1;
    }

    //计算枪管到目标装甲板yaw小的那个装甲板

    float timeDelay = (float)((timestamp_now - timestamp_start)/1000.0) + t; 
    // float timeDelay = t; //子弹飞行时间
    zw = zw + vzw * timeDelay;
    *pitch = -GimbalControlGetPitch(sqrt((xw) * (xw) + (yw) * (yw)) + x_static, zw - z_static, st.current_v);
    // *pitch = - atan2(zw - z_static, sqrt((xw) * (xw) + (yw) * (yw)) - x_static); //单纯用于跟随
    xw = xw + vxw * timeDelay;
    yw = yw + vyw * timeDelay ;
    *yaw = (float)(atan2(yw, xw));

    *aim_x = tar_position[0].x + vxw * timeDelay;
    *aim_y = tar_position[0].y + vyw * timeDelay;
    *aim_z = tar_position[0].z + vzw * timeDelay;

    }

// 从坐标轴正向看向原点，逆时针方向为正

int main()
{
    float tar_x = 0.3, tar_y = 0, tar_z = 0.08;    // target point  s = sqrt(x^2+y^2)
    // float tar_x = 0.5, tar_y = 0.866, tar_z = 0.5;    // target point  s = sqrt(x^2+y^2)
    // float tar_x = 0.5, tar_y = 0.866, tar_z = -0.1;    // target point  s = sqrt(x^2+y^2)
    float *aim_x = 0, *aim_y = 0, *aim_z = 0; // aim point
    float tar_vx = 0, tar_vy = 0, tar_vz = 0; // target velocity
    float pitch = 0;
    float yaw = 0;
    int timestamp = 1;
    // 机器人初始状态
    GimbalControlInit(0, 0, 0, 0.2 ,0.2, 0 ,18, 0.076);
    // GimbalControlInit(0, 0, 18, 0.1);
    // GimbalControlInit(0, 0, 18, 0.067);
    
    GimbalControlTransform(tar_x, tar_y, tar_z, tar_vx, tar_vy, tar_vz, timestamp, &pitch, &yaw, &aim_x, &aim_y, &aim_z);


    printf("main pitch:%f° yaw:%f° ", pitch * 180 / PI, yaw * 180 / PI);
    printf("\npitch:%frad yaw:%frad", pitch, yaw);

    return 0;
}
