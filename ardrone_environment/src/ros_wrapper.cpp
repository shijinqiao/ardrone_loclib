#include "ros_wrapper.h"

#include "environmentengine.h"

#include "opencv/cv.h"
#include "ros/ros.h"
#include "image_transport/image_transport.h"
#include "opencv/cvwimage.h"
#include "cv_bridge/CvBridge.h"

EnvironmentEngine* ros_wrapper::p_environment = 0;

ros_wrapper::ros_wrapper(QObject *parent) :
    QThread(parent)
{
    m_isRunning = true;
}

/** @brief thread function which is a ros loop that listens and publishes topics.
  */
void ros_wrapper::run()
{
    ros::NodeHandle n;
    image_transport::ImageTransport it(n);
    ros::Rate loop_rate(5);

    ros::ServiceServer s1 = n.advertiseService("ardrone_loclib_map", ros_wrapper::sendStaticMap);
    ros::ServiceServer s2 = n.advertiseService("ardrone_loclib_tags", ros_wrapper::sendTagList);

    image_transport::Publisher environmentPublisher = it.advertise("ardrone_loclib_image", 100);
    sensor_msgs::ImagePtr msg;

    ros::Subscriber su1 = n.subscribe("ardrone_loclib_mission", 1000, ros_wrapper::storeMission);
    ros::Subscriber su2 = n.subscribe("ardrone_loclib_position2d", 1000, ros_wrapper::locateDrone);
    IplImage *img;

    while(ros::ok()){
        if(p_environment != 0 && p_environment->ready())
        {
            p_environment->computeImage();
            img = p_environment->getCvImage();
            //msg = sensor_msgs::CvBridge::cvToImgMsg(img, "rgba8");
            //environmentPublisher.publish(msg);
            emit environmentImagePublished(img);
        }

        ros::spinOnce();

        loop_rate.sleep();

    }
}

/** @brief Ends the ros node
  */
void ros_wrapper::end()
{
    ros::shutdown();
}

/** @brief Stores a pointer on the environment engine in order to call methods when the wrapper receives data from ROS
  */
void ros_wrapper::storeEnvironmentEngine(EnvironmentEngine *env)
{
    ros_wrapper::p_environment = env;
}


/** @brief Sends a 2d array of the tiles composing the map (Wall or empty tile) on the ros bus
  *
  */
bool ros_wrapper::sendStaticMap(ardrone_environment::ARDroneMapSrv::Request &req, ardrone_environment::ARDroneMapSrv::Response &res)
{
    qDebug("sending back response");
    if(ros_wrapper::p_environment == 0) return true;

    int width = p_environment->getSize().width();
    int height = p_environment->getSize().height();
    res.map.tiles.resize(width);
    res.map.name = "INSA map";

    for(int x=0; x < width; x++)
    {
        res.map.tiles.at(x).x.resize(height);

        for(int y=0; y<height; y++)
        {
            res.map.tiles.at(x).x.at(y) = p_environment->getTile(x, y);
        }
    }

    return true;
}

/** @brief Sends the list of the tags on the map
  *
  */
bool ros_wrapper::sendTagList(ardrone_environment::ARDroneTagListSrv::Request &req, ardrone_environment::ARDroneTagListSrv::Response &res)
{
    res.tagList.resize(p_environment->getTagsList().size());
    for(int i=0; i<p_environment->getTagsList().size(); i++)
    {
        EnvironmentEngine::Tag tag = p_environment->getTagsList().at(i);
        res.tagList.at(i).x = tag.x;
        res.tagList.at(i).y = tag.y;
    }

    return true;
}

/** @brief Callback called when the environment receives a path made of all the 2D points the drone will fly over to reach a target (the last point)
  *
  */
void ros_wrapper::storeMission(const ardrone_environment::ARDroneMission::ConstPtr &mission)
{
    qDebug("Store mission callback");
    EnvironmentEngine::Path p;
    for(unsigned int i=0; i<mission->positions.size(); i++)
    {
        p.append(EnvironmentEngine::DoublePoint(mission->positions.at(i).x, mission->positions.at(i).y));
    }
    p_environment->setMission(p);
}

/** @brief Gets the drone position from the topic and refresh its position in the engine.
  */
void ros_wrapper::locateDrone(const ardrone_environment::ARDronePosition2D::ConstPtr &msg)
{
    p_environment->updateDronePosition(msg->x, msg->y);
}
