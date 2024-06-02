// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2023 Droidian Project
//
// Authors:
// Bardia Moshiri <fakeshell@bardia.tech>
// Erik Inkinen <erik.inkinen@gmail.com>
// Alexander Rutz <alex@familyrutz.com>
// Joaquin Philco <joaquinphilco@gmail.com>

#include "filemanager.h"
#include "exif.h"
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QProcess>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDateTime>
#include <QDebug>
#include <iomanip>

FileManager::FileManager(QObject *parent) : QObject(parent) {

    struct geoClueProperties {
        float Latitude;
        float Longitude;
        float Accuracy;
        float Altitude;
        float Speed;
        float Heading;
        QString Description;

        struct Timestamp {
            int time1, time2;
        };
    };

    const QString service = "org.freedesktop.GeoClue2";
    const QString path = "/org/freedesktop/GeoClue2/Manager";
    const QString interface = "org.freedesktop.GeoClue2.Manager";
    const QString property = "Locked";

    QDBusConnection dbusConnection = QDBusConnection::systemBus();

    QDBusInterface dbusInterface(service, path, interface, dbusConnection);
    if (!dbusInterface.isValid()) {
        qWarning() << "\n \n \nD-Bus interface is not valid!";
    } else {
        qDebug() << "\n \n \nConnected to DBus service:" << service;
    }
  
    QDBusReply<QDBusObjectPath> reply = dbusInterface.call("GetClient");
    if (!reply.isValid()) {
        qWarning() << "\n \n \nDBus call failed: " << reply.error().message();
    } else {
        qDebug() << "\n \n \nGot the geoclue client created.";
    }

    QString objPath = reply.value().path();

    QDBusInterface clientInterface(service,objPath, "org.freedesktop.GeoClue2.Client", dbusConnection);
    if (!clientInterface.isValid()) {
        qWarning() << "\n \n \nD-Bus interface is not valid!";
    } else {
        qDebug() << "\n \n \nConnected to client DBus service: " << service;
    }

    QDBusReply<void> startReply = clientInterface.call("Start");
    if (!reply.isValid()) {
        qWarning() << "DBus call failed: " << startReply.error().message();
    } else {
        qDebug() << "\n \n \nStart Call from " << service;
    }



    QDBusInterface propertyInterface(service, objPath, "org.freedesktop.DBus.Properties", dbusConnection);
    if (!propertyInterface.isValid()) {
        qWarning() << "\n \n \nD-Bus interface is not valid!";
    } else {
        qDebug() << "\n \n \nConnected to client DBus service: " << objPath;
    }

    QDBusReply<void> setPropertyReply = propertyInterface.call("Set", "Properties", "DesktopId", "CameraApp");
    if (!setPropertyReply.isValid()) {
        qWarning() << "\n \n \nDBus call failed: " << setPropertyReply.error().message();
    } else {
        qDebug() << "\n \n \nSet Desktop Id to CameraApp.";
    }
}

// void handleLocationUpdated(const QDBusMessage &message) {
//     qDebug() << "Location updated signal received:" << message.arguments();
// }

// ***************** File Management *****************

void FileManager::createDirectory(const QString &path) {
    QDir dir;

    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!dir.exists(homePath + path)) {
        dir.mkpath(homePath + path);
    }
}

void FileManager::removeGStreamerCacheDirectory() {
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString filePath = homePath + "/.cache/gstreamer-1.0/registry.aarch64.bin";
    QDir dir(homePath + "/.cache/gstreamer-1.0/");

    QFile file(filePath);

    if (file.exists()) {
         QFileInfo fileInfo(file);
         QDateTime lastModified = fileInfo.lastModified();

         if (lastModified.addDays(7) < QDateTime::currentDateTime()) {
             dir.removeRecursively();
         }
    }
}

QString FileManager::getConfigFile() {
    QFileInfo primaryConfig("/usr/lib/droidian/device/droidian-camera.conf");
    QFileInfo secodaryConfig("/etc/droidian-camera.conf");

    if (primaryConfig.exists()) {
        return primaryConfig.absoluteFilePath();
    } else if (secodaryConfig.exists()) {
        return secodaryConfig.absoluteFilePath();
    } else {
        return "None";
    }
}

bool FileManager::deleteImage(const QString &fileUrl) {
    QString path = fileUrl;
    int colonIndex = path.indexOf(':');

    if (colonIndex != -1) {
        path.remove(0, colonIndex + 1);
    }

    QFile file(path);

    return file.exists() && file.remove();
}

// ***************** Picture Metada *****************

easyexif::EXIFInfo FileManager::getPictureMetaData(const QString &fileUrl){

    getCurrentLocation();

    QString path = fileUrl;
    int colonIndex = path.indexOf(':');

    if (colonIndex != -1) {
        path.remove(0, colonIndex + 1);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Can't open file.");
    }

    QByteArray fileContent = file.readAll();
    if (fileContent.isEmpty()) {
        qWarning("Can't read file.");
    }
    file.close();

    easyexif::EXIFInfo result;
    int code = result.parseFrom(reinterpret_cast<unsigned char*>(fileContent.data()), fileContent.size());
    if (code) {
        qWarning() << "Error parsing EXIF: code" << code;
    }

    return result;
}

QString FileManager::getPictureDate(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    std::tm tm = {};
    std::istringstream ss(metadata.DateTime);

    ss >> std::get_time(&tm, "%Y:%m:%d %H:%M:%S");
    if (ss.fail()) {
        return "Invalid date/time";
    }

    char buffer[80];
    // Formats to "Month day Year    HH:mm"
    strftime(buffer, sizeof(buffer), "%b %d, %Y \n %H:%M", &tm);
    return QString::fromStdString(buffer);
}

QString FileManager::getCameraHardware(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    std::string make = metadata.Make;
    std::string model = metadata.Model;

    return QString("%1 %2").arg(QString::fromStdString(make)).arg(QString::fromStdString(model));
}

QString FileManager::getDimensions(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    int width = metadata.ImageWidth;
    int height = metadata.ImageHeight;

    return QString("%1 x %2").arg(QString::number(width)).arg(QString::number(height));
}

QString FileManager::getFStop(const QString &fileUrl) { // Aperture settings

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    float fNumber = metadata.FNumber;

    return QString("f/%1").arg(QString::number(fNumber));
}

QString FileManager::getExposure(const QString &fileUrl) { // Exposure Time

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    unsigned int exposure = static_cast<unsigned int>(1.0 / metadata.ExposureTime);

    return QString("1/%1 s").arg(QString::number(exposure));
}

QString FileManager::getISOSpeed(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    int iso = metadata.ISOSpeedRatings;

    return QString("ISO: %1").arg(QString::number(iso));
}

QString FileManager::getExposureBias(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    float exposureBias = metadata.ExposureBiasValue;


    return QString("%1 EV").arg(QString::number(exposureBias));
}
QString FileManager::focalLengthStandard(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    unsigned short focalLength = metadata.FocalLengthIn35mm;

    return QString("35mm focal length: %1 mm").arg(QString::number(focalLength));
}

QString FileManager::focalLength(const QString &fileUrl) {

    if (fileUrl == "") {
        return QString("");
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);

    float focalLength = metadata.FocalLength;

    return QString("%1 mm").arg(QString::number(focalLength));
}

bool FileManager::getFlash(const QString &fileUrl) {

    if (fileUrl == "") {
        return false;
    }

    easyexif::EXIFInfo metadata = getPictureMetaData(fileUrl);
    
    return  metadata.Flash == '1';
}

// ***************** Video Metadata *****************

void FileManager::getVideoMetadata(const QString &fileUrl) {
    QStringList metadataList;
    qDebug() << "Requesting Date for Video";

    QString path = fileUrl;
    int colonIndex = path.indexOf(':');

    if (colonIndex != -1) {
        path.remove(0, colonIndex + 1);
    }

    QProcess process;
    process.setProgram("mkvinfo");
    process.setArguments(QStringList() << path);

    process.start();
    if (!process.waitForFinished()) {
        qDebug() << "Error executing mkvinfo:" << process.errorString();
        return;
    }

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    if (!errorOutput.isEmpty()) {
        qDebug() << "mkvinfo error output:" << errorOutput;
    }

    qDebug() << "Full mkvinfo output:" << output;

    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Duration") || line.contains("Title") ||
            line.contains("Muxing application") || line.contains("Writing application") ||
            line.contains("Track number") || line.contains("Track type") ||
            line.contains("Codec ID") || line.contains("Pixel width") ||
            line.contains("Pixel height") || line.contains("Channels") ||
            line.contains("Sampling frequency") || line.contains("Date")) {
            metadataList << line.trimmed();
        }
    }

    qDebug() << "Metadata Tags:";
    for (const QString &info : metadataList) {
        qDebug() << info;
    }
}

QString FileManager::runMkvInfo(const QString &fileUrl) {
    QString path = fileUrl;
    int colonIndex = path.indexOf(':');
    if (colonIndex != -1) {
        path.remove(0, colonIndex + 1);
    }

    QProcess process;
    process.setProgram("mkvinfo");
    process.setArguments(QStringList() << path);
    process.start();
    if (!process.waitForFinished()) {
        qDebug() << "Error executing mkvinfo:" << process.errorString();
        return "";
    }

    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    if (!errorOutput.isEmpty()) {
        qDebug() << "mkvinfo error output:" << errorOutput;
    }

    return output;
}

QString FileManager::getVideoDate(const QString &fileUrl) {
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Date")) {
            QString dateLine = line.trimmed();
            QString dateTimeStr = dateLine.section(':', 1).trimmed();
            QDateTime dateTime = QDateTime::fromString(dateTimeStr, "yyyy-MM-dd HH:mm:ss t");
            if (dateTime.isValid()) {
                return dateTime.toString("MMM d, yyyy \n HH:mm");
            }
            break;
        }
    }
    return QString("Date not found.");
}

QString FileManager::getVideoDimensions(const QString &fileUrl) {
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    QString width, height;

    for (const QString &line : outputLines) {
        if (line.contains("Pixel width")) {
            width = line.split(':').last().trimmed();
        } else if (line.contains("Pixel height")) {
            height = line.split(':').last().trimmed();
        }
    }

    if (!width.isEmpty() && !height.isEmpty()) {
        return QString("%1x%2").arg(width).arg(height);
    } else {
        qDebug() << "Dimensions not found.";
        return QString("Dimensions not found.");
    }
}

QString FileManager::getDuration(const QString &fileUrl) {
    qDebug() << "Video Component";
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Duration")) {
            QString string = QString( "Duration: ") + line.trimmed();
            qDebug() << string;
            return string;
        }
    }
    qDebug() << "Duration not found.";
    return QString("Duration not found.");
}

QString FileManager::getMultiplexingApplication(const QString &fileUrl) {
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Multiplexing application:")) {
            QString multiplexingApplication = line.split(':').last().trimmed();
            return QString("%1").arg(multiplexingApplication);
        }
    }
    return QString("Multiplexing Application: Not found");
}

QString FileManager::getWritingApplication(const QString &fileUrl) {
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Writing application")) {
            QString string =  line.trimmed();
            return string;
        }
    }
    qDebug() << "Writing application not found.";
    return "";
}

QString FileManager::getDocumentType(const QString &fileUrl) {
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Document type:")) {
            QString documentType = line.split(':').last().trimmed();
            return QString("File Type: %1").arg(documentType);
        }
    }
    return QString("File Type: Not found");
}


QString FileManager::getCodecId(const QString &fileUrl) {
    QString output = runMkvInfo(fileUrl);
    QStringList outputLines = output.split('\n');
    for (const QString &line : outputLines) {
        if (line.contains("Codec ID:")) {
            QString codecId = line.split(':').last().trimmed();
            return QString("Codec ID: %1").arg(codecId);
        }
    }
    return QString("Codec ID: Not found");
}

// ***************** GPS Metadata *****************

void FileManager::getCurrentLocation() {


    // const QString service = "org.freedesktop.GeoClue2";
    // const QString path = "/org/freedesktop/GeoClue2/Manager";
    // const QString interface = "org.freedesktop.GeoClue2.Manager";
    // const QString property = "Locked";

    // QDBusInterface dbusInterface(service, path, "org.freedesktop.GeoClue2.Manager", QDBusConnection::sessionBus());
    // if (!dbusInterface.isValid()) {
    //     qWarning() << "D-Bus interface is not valid!";
    //     //return false;
    // }

    // double new_lat, new_lon;

    // qDebug() << "Connected to DBus service: " << service;

    //Location interface appears if you have an object of the client;




    // QDBusReply<QVariant> new_lat = dbusInterface.call("Latitude", interface, property);
    // if (reply.isValid()) {
    //     bool isLocked = reply.value().toBool();
    //     //return isLocked;
    // } else {
    //     qWarning() << "Failed to get property:" << reply.error().message();
    //     //return false;
    // }
}