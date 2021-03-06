/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef MULTIGETJOB_H
#define MULTIGETJOB_H

#include "transferjob.h"

namespace KIO
{

class MultiGetJobPrivate;
/**
 * @class KIO::MultiGetJob multigetjob.h <KIO/MultiGetJob>
 *
 * The MultiGetJob is a TransferJob that allows you to get
 * several files from a single server. Don't create directly,
 * but use KIO::multi_get() instead.
 * @see KIO::multi_get()
 */
class KIOCORE_EXPORT MultiGetJob : public TransferJob
{
    Q_OBJECT

public:
    ~MultiGetJob() override;

    /**
     * Get an additional file.
     *
     * @param id the id of the file
     * @param url the url of the file to get
     * @param metaData the meta data for this request
     */
    void get(long id, const QUrl &url, const MetaData &metaData);

Q_SIGNALS:
    /**
     * Data from the slave has arrived.
     * @param id the id of the request
     * @param data data received from the slave.
     * End of data (EOD) has been reached if data.size() == 0
     */
    void data(long id, const QByteArray &data);

    /**
     * Mimetype determined
     * @param id the id of the request
     * @param type the mime type
     */
    void mimetype(long id, const QString &type);

    /**
     * File transfer completed.
     *
     * When all files have been processed, result(KJob *) gets
     * emitted.
     * @param id the id of the request
     */
    void result(long id);

protected Q_SLOTS:
    void slotRedirection(const QUrl &url) override;
    void slotFinished() override;
    void slotData(const QByteArray &data) override;
    void slotMimetype(const QString &mimetype) override;

protected:
    MultiGetJob(MultiGetJobPrivate &dd);
private:
    Q_DECLARE_PRIVATE(MultiGetJob)
};

/**
 * Creates a new multiple get job.
 *
 * @param id the id of the get operation
 * @param url the URL of the file
 * @param metaData the MetaData associated with the file
 *
 * @return the job handling the operation.
 * @see get()
 */
KIOCORE_EXPORT MultiGetJob *multi_get(long id, const QUrl &url, const MetaData &metaData);

}

#endif
