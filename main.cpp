#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

struct Annotation {
    QString label;
    QRectF box; // normalized x, y, width, height
};

class ImageCanvas final : public QWidget {
public:
    explicit ImageCanvas(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(480, 360);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setImage(const QImage &image) {
        image_ = image;
        zoom_ = 1.0;
        pan_ = {};
        update();
    }
    void setAnnotations(const QVector<Annotation> &items) { annotations_ = items; update(); }
    void setSelectedAnnotation(int index) { selected_ = index; update(); }
    void setCurrentLabel(const QString &label) { currentLabel_ = label; }
    bool hasImage() const { return !image_.isNull(); }
    std::function<void(const QRectF &)> onBox;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(32, 34, 38));
        if (!hasImage()) {
            p.setPen(QColor(180, 185, 190));
            p.drawText(rect(), Qt::AlignCenter, "Open an image or start the camera");
            return;
        }
        const qreal fit = std::min(width() / qreal(image_.width()), height() / qreal(image_.height()));
        const QSizeF shown(image_.width() * fit * zoom_, image_.height() * fit * zoom_);
        imageRect_ = QRectF((width() - shown.width()) / 2 + pan_.x(), (height() - shown.height()) / 2 + pan_.y(), shown.width(), shown.height());
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(imageRect_, image_);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < annotations_.size(); ++i) {
            const auto &a = annotations_[i];
            QRectF r(imageRect_.x() + a.box.x() * imageRect_.width(), imageRect_.y() + a.box.y() * imageRect_.height(), a.box.width() * imageRect_.width(), a.box.height() * imageRect_.height());
            p.setPen(QPen(i == selected_ ? QColor(255, 210, 65) : QColor(50, 220, 130), i == selected_ ? 3 : 2));
            p.drawRect(r);
            p.fillRect(QRectF(r.topLeft(), QSizeF(std::max(55, int(a.label.size()) * 8), 20)), QColor(0, 0, 0, 170));
            p.setPen(Qt::white);
            p.drawText(QRectF(r.x() + 4, r.y(), 200, 20), Qt::AlignVCenter, a.label);
        }
        if (drawing_) {
            p.setPen(QPen(QColor(255, 210, 65), 2, Qt::DashLine));
            p.drawRect(QRectF(start_, cursor_).normalized());
        }
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (!hasImage()) return;
        if (e->button() == Qt::RightButton || e->button() == Qt::MiddleButton) { panning_ = true; last_ = e->pos(); return; }
        if (e->button() != Qt::LeftButton || !imageRect_.contains(e->pos())) return;
        start_ = cursor_ = e->pos();
        drawing_ = true;
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (panning_) { pan_ += e->pos() - last_; last_ = e->pos(); update(); }
        else if (drawing_) { cursor_ = e->pos(); update(); }
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::RightButton || e->button() == Qt::MiddleButton) { panning_ = false; return; }
        if (!drawing_) return;
        drawing_ = false;
        const QRectF r = QRectF(start_, e->pos()).normalized().intersected(imageRect_);
        if (r.width() > 5 && r.height() > 5 && onBox) {
            onBox(QRectF((r.x() - imageRect_.x()) / imageRect_.width(), (r.y() - imageRect_.y()) / imageRect_.height(), r.width() / imageRect_.width(), r.height() / imageRect_.height()));
        }
        update();
    }
    void wheelEvent(QWheelEvent *e) override {
        if (!hasImage()) return;
        zoom_ = std::clamp(zoom_ * (e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15), 0.2, 8.0);
        update();
    }

private:
    QImage image_;
    QVector<Annotation> annotations_;
    QString currentLabel_;
    QRectF imageRect_;
    QPointF pan_;
    QPoint start_, cursor_, last_;
    qreal zoom_ = 1.0;
    int selected_ = -1;
    bool drawing_ = false, panning_ = false;
};

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("Vision Labeler");
        resize(1280, 800);
        auto *root = new QWidget;
        auto *layout = new QHBoxLayout(root);
        auto *left = new QWidget;
        left->setFixedWidth(270);
        auto *side = new QVBoxLayout(left);
        auto *actions = new QHBoxLayout;
        auto *open = button("Open images");
        auto *folder = button("Open folder");
        actions->addWidget(open); actions->addWidget(folder);
        side->addLayout(actions);
        filter_ = new QLineEdit;
        filter_->setPlaceholderText("Filter images...");
        side->addWidget(filter_);
        files_ = new QListWidget;
        side->addWidget(files_, 1);
        auto *cameraRow = new QHBoxLayout;
        auto *camera = button("Camera");
        auto *capture = button("Capture");
        cameraRow->addWidget(camera); cameraRow->addWidget(capture);
        side->addLayout(cameraRow);
        side->addWidget(new QLabel("Annotation class"));
        classes_ = new QComboBox;
        classes_->setEditable(true);
        classes_->addItems({"object"});
        side->addWidget(classes_);
        annotations_ = new QListWidget;
        side->addWidget(annotations_, 1);
        auto *editRow = new QHBoxLayout;
        auto *rename = button("Rename");
        auto *remove = button("Delete box");
        editRow->addWidget(rename); editRow->addWidget(remove);
        side->addLayout(editRow);
        auto *exportRow = new QHBoxLayout;
        auto *yolo = button("Export YOLO");
        auto *json = button("Save JSON");
        exportRow->addWidget(yolo); exportRow->addWidget(json);
        side->addLayout(exportRow);
        canvas_ = new ImageCanvas;
        layout->addWidget(left);
        layout->addWidget(canvas_, 1);
        setCentralWidget(root);
        statusBar()->showMessage("Drag on the image to create a box · wheel to zoom · right-drag to pan");

        connect(open, &QPushButton::clicked, this, [this] { openImages(); });
        connect(folder, &QPushButton::clicked, this, [this] { openFolder(); });
        connect(filter_, &QLineEdit::textChanged, this, [this] { rebuildList(); });
        connect(files_, &QListWidget::currentRowChanged, this, [this](int row) { selectFile(row); });
        connect(camera, &QPushButton::clicked, this, [this] { toggleCamera(); });
        connect(capture, &QPushButton::clicked, this, [this] { captureFrame(); });
        connect(rename, &QPushButton::clicked, this, [this] { renameAnnotation(); });
        connect(remove, &QPushButton::clicked, this, [this] { deleteAnnotation(); });
        connect(annotations_, &QListWidget::currentRowChanged, this, [this](int row) { canvas_->setSelectedAnnotation(row); });
        connect(yolo, &QPushButton::clicked, this, [this] { exportYolo(); });
        connect(json, &QPushButton::clicked, this, [this] { saveJson(); });
        canvas_->onBox = [this](const QRectF &box) { if (!cameraTimer_.isActive()) addAnnotation(box); };
        cameraTimer_.setInterval(33);
        connect(&cameraTimer_, &QTimer::timeout, this, [this] { readCamera(); });
        new QShortcut(QKeySequence::Delete, this, [this] { deleteAnnotation(); });
        new QShortcut(QKeySequence(Qt::Key_Right), this, [this] { files_->setCurrentRow(std::min(files_->currentRow() + 1, files_->count() - 1)); });
        new QShortcut(QKeySequence(Qt::Key_Left), this, [this] { files_->setCurrentRow(std::max(0, files_->currentRow() - 1)); });
    }

private:
    static QPushButton *button(const QString &text) { return new QPushButton(text); }
    QString currentPath() const { return files_->currentItem() ? files_->currentItem()->data(Qt::UserRole).toString() : QString(); }
    QVector<Annotation> &currentAnnotations() { return labels_[currentPath()]; }

    void openImages() {
        const auto paths = QFileDialog::getOpenFileNames(this, "Open images", {}, "Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)");
        for (const auto &path : paths) addFile(path);
    }
    void openFolder() {
        const auto dir = QFileDialog::getExistingDirectory(this, "Open image folder");
        if (dir.isEmpty()) return;
        QDir d(dir);
        const auto entries = d.entryInfoList({"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff"}, QDir::Files, QDir::Name);
        for (const auto &entry : entries) addFile(entry.absoluteFilePath());
    }
    void addFile(const QString &path) {
        if (path.isEmpty() || !QImageReader(path).canRead() || paths_.contains(path)) return;
        paths_.append(path);
        if (labels_.contains(path)) return;
        labels_.insert(path, {});
        rebuildList();
        if (files_->count() && files_->currentRow() < 0) files_->setCurrentRow(0);
    }
    void rebuildList() {
        const QString keep = currentPath();
        files_->clear();
        for (const auto &path : paths_) if (QFileInfo(path).fileName().contains(filter_->text(), Qt::CaseInsensitive)) {
            auto *item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setData(Qt::UserRole, path);
            files_->addItem(item);
            if (path == keep) files_->setCurrentItem(item);
        }
        if (files_->currentRow() < 0 && files_->count()) files_->setCurrentRow(0);
    }
    void selectFile(int row) {
        if (row < 0 || row >= files_->count()) return;
        stopCamera();
        const QString path = currentPath();
        QImage image(path);
        if (image.isNull()) { statusBar()->showMessage("Could not load " + path); return; }
        canvas_->setImage(image);
        refreshAnnotations();
        statusBar()->showMessage(QString("%1 × %2  ·  %3").arg(image.width()).arg(image.height()).arg(QFileInfo(path).fileName()));
    }
    void refreshAnnotations() {
        annotations_->clear(); canvas_->setSelectedAnnotation(-1);
        const QString path = currentPath();
        if (path.isEmpty()) { canvas_->setAnnotations({}); return; }
        auto &items = labels_[path];
        for (const auto &a : items) annotations_->addItem(a.label);
        canvas_->setAnnotations(items);
    }
    void addAnnotation(const QRectF &box) {
        if (currentPath().isEmpty()) return;
        QString label = classes_->currentText().trimmed();
        if (label.isEmpty()) label = "object";
        if (classes_->findText(label) < 0) classes_->addItem(label);
        currentAnnotations().append({label, box});
        refreshAnnotations();
        annotations_->setCurrentRow(annotations_->count() - 1);
    }
    void renameAnnotation() {
        const int i = annotations_->currentRow();
        if (i < 0 || currentPath().isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, "Edit class", "Class name:", QLineEdit::Normal, currentAnnotations()[i].label, &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        currentAnnotations()[i].label = name;
        if (classes_->findText(name) < 0) classes_->addItem(name);
        refreshAnnotations(); annotations_->setCurrentRow(i);
    }
    void deleteAnnotation() {
        const int i = annotations_->currentRow();
        if (i < 0 || currentPath().isEmpty()) return;
        currentAnnotations().removeAt(i);
        refreshAnnotations();
    }
    void toggleCamera() {
        if (cameraTimer_.isActive()) { stopCamera(); return; }
        if (!camera_.open(0)) { QMessageBox::warning(this, "Camera", "Could not open camera 0."); return; }
        cameraTimer_.start(); statusBar()->showMessage("Live camera · press Capture to save a frame");
    }
    void stopCamera() { cameraTimer_.stop(); camera_.release(); }
    void readCamera() {
        cv::Mat frame;
        if (!camera_.read(frame) || frame.empty()) return;
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        cameraFrame_ = QImage(frame.data, frame.cols, frame.rows, int(frame.step), QImage::Format_RGB888).copy();
        canvas_->setImage(cameraFrame_);
    }
    void captureFrame() {
        if (cameraFrame_.isNull()) { statusBar()->showMessage("Start the camera first"); return; }
        const QString path = QFileDialog::getSaveFileName(this, "Save captured image", "capture.jpg", "JPEG (*.jpg);;PNG (*.png)");
        if (path.isEmpty()) return;
        if (!cameraFrame_.save(path)) { QMessageBox::warning(this, "Save image", "Could not save the image."); return; }
        filter_->clear();
        addFile(path);
        for (int i = 0; i < files_->count(); ++i) if (files_->item(i)->data(Qt::UserRole).toString() == path) files_->setCurrentRow(i);
        statusBar()->showMessage("Captured " + path);
    }
    int classId(const QString &label) {
        int i = classes_->findText(label);
        if (i < 0) { classes_->addItem(label); i = classes_->count() - 1; }
        return i;
    }
    void exportYolo() {
        const QString root = QFileDialog::getExistingDirectory(this, "Choose dataset output folder");
        if (root.isEmpty()) return;
        QDir out(root); out.mkpath("images"); out.mkpath("labels");
        for (const auto &path : paths_) for (const auto &a : labels_[path]) classId(a.label);
        QStringList names;
        for (int i = 0; i < classes_->count(); ++i) names << classes_->itemText(i);
        for (const auto &path : paths_) {
            const QFileInfo info(path);
            const QString stem = info.completeBaseName();
            QFile::copy(path, out.filePath("images/" + info.fileName()));
            QFile f(out.filePath("labels/" + stem + ".txt"));
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) continue;
            QTextStream s(&f);
            for (const auto &a : labels_[path]) {
                const qreal cx = a.box.x() + a.box.width() / 2;
                const qreal cy = a.box.y() + a.box.height() / 2;
                s << classId(a.label) << ' ' << QString::number(cx, 'f', 6) << ' ' << QString::number(cy, 'f', 6) << ' ' << QString::number(a.box.width(), 'f', 6) << ' ' << QString::number(a.box.height(), 'f', 6) << '\n';
            }
        }
        QFile classesFile(out.filePath("classes.txt"));
        if (classesFile.open(QIODevice::WriteOnly | QIODevice::Text)) { QTextStream s(&classesFile); for (const auto &n : names) s << n << '\n'; }
        QMessageBox::information(this, "Export complete", "YOLO dataset written to:\n" + root);
    }
    void saveJson() {
        const QString path = QFileDialog::getSaveFileName(this, "Save annotations", "annotations.json", "JSON (*.json)");
        if (path.isEmpty()) return;
        QJsonArray images;
        for (const auto &file : paths_) {
            QJsonArray boxes;
            for (const auto &a : labels_[file]) boxes.append(QJsonObject{{"class", a.label}, {"x", a.box.x()}, {"y", a.box.y()}, {"width", a.box.width()}, {"height", a.box.height()}});
            images.append(QJsonObject{{"image", file}, {"annotations", boxes}});
        }
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) { QMessageBox::warning(this, "Save JSON", "Could not write the file."); return; }
        f.write(QJsonDocument(QJsonObject{{"images", images}}).toJson(QJsonDocument::Indented));
        statusBar()->showMessage("Saved " + path);
    }

    ImageCanvas *canvas_ = nullptr;
    QListWidget *files_ = nullptr, *annotations_ = nullptr;
    QLineEdit *filter_ = nullptr;
    QComboBox *classes_ = nullptr;
    QStringList paths_;
    QHash<QString, QVector<Annotation>> labels_;
    cv::VideoCapture camera_;
    QTimer cameraTimer_;
    QImage cameraFrame_;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
